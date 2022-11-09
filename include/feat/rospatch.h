#ifdef PATCH_ROS
static bool stop_ros = false;
static bool ros_patching = false;

#define ROS0_overlay_path "/dev_hdd0/ros0_overlay.bin"
#define ROS1_overlay_path "/dev_hdd0/ros1_overlay.bin"
#define PRINT_BUFFER_LEN 128

static int sys_storage_write(int fd, u32 start_sector, u32 sectors, void *bounce_buf, u32 *sectors_read)
{
    system_call_7(603, fd, 0, start_sector, sectors, (u64)(u32) bounce_buf, (u64)(u32) sectors_read, FLASH_FLAGS);
    return (int)p1;
}

static void sys_ss_get_cache_of_flash_ext_flag(u8 *flag)
{
    system_call_1(874, (u64)(u32) flag);
}

static bool is_nor(void)
{
    u8 flag = 0;
    sys_ss_get_cache_of_flash_ext_flag(&flag);
    return !(1 & flag);
}

static void ssend_message(int socket, const char* format, ...)
{
    char buf[PRINT_BUFFER_LEN];
    va_list args;

    va_start(args, format);
    vsnprintf(buf, PRINT_BUFFER_LEN, format, args);
    va_end(args);

    send(socket, buf, strlen(buf), 0);
}

// "Patch hash" terminology is kindof stupid imo, but I can't think of a better name
static void ros_checksum(int flash_id, int start_sec, int offset, char *region_hash, char *patch_hash)
{
    char read_buf[512];
    CellMd5WorkArea ros_region;
    CellMd5WorkArea ros_patch;
    cellMd5BlockInit(&ros_region);
    cellMd5BlockInit(&ros_patch);

    int bytes_left = 7340000;
    for(int sector = 0; sector <= 0x37FF; sector++){
        u32 r;
        sys_storage_read(flash_id, 0, start_sec + sector, 1, &read_buf, &r, FLASH_FLAGS);

        cellMd5BlockUpdate(&ros_region, &read_buf, 512);
        int skip_offset = (sector == 0) ? offset : 0;
        int len = 512 - skip_offset;
        if(len > bytes_left) len = bytes_left;
        cellMd5BlockUpdate(&ros_patch, &read_buf[skip_offset], len);
        bytes_left -= len;

        if(sector == 0x37FF && bytes_left == 16)
        {
            // NAND ROS0 happens to overflow to another sector by 16 bytes
            // so we push in 16 zeroes manually
            char zero[16] = { 0 };
            cellMd5BlockUpdate(&ros_patch, &zero, 16);
        }
    }

    u64 _hash_region[2] = {0, 0};
    u64 _hash_patch[2] = {0, 0};
    cellMd5BlockResult(&ros_region, (u8*)_hash_region);
    cellMd5BlockResult(&ros_patch, (u8*)_hash_patch);
    sprintf(region_hash, "%016llx%016llx", _hash_region[0], _hash_region[1]);
    sprintf(patch_hash, "%016llx%016llx", _hash_patch[0], _hash_patch[1]);
}

// TODO: resolve unused "full", add missing bool usb_debug
static int patch_ros(const char *patch_file, u8 *mem, int conn_s, u8 ros, bool full)
{
    ssend_message(conn_s, "<p>Patch process start</p>");
    if(ros_patching || file_size(patch_file) != 7340000) {BEEP3; return FAILED;}

    const char ROS_HEADER[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6F, 0xFF, 0xE0 };

    const char *overlay_path[] = { ROS0_overlay_path, ROS1_overlay_path };

    char expected_ros_reg_hash[2][33];
    char expected_patch_hash[33];
    u32 ros_offset[2];
    u32 ros_start_sector[2];

    ros_patching = true; // prevent call this function while processing

    // Redirect all flash reads and writes to dev_usb000
    // USB storage is supposed to contain a flash dump written directly from the beginning
    // i.e. dd if=nor_dump.bin of=/dev/sde     or     Win32DiskImager / ImageUSB for Windows
    bool usb_debug = true;

    // open nofsm_patch.bin
    int fd, ret = CELL_OK;
    if(cellFsOpen(patch_file, CELL_FS_O_RDONLY, &fd, NULL, 0) != CELL_FS_SUCCEEDED) {BEEP3; ros_patching = false; return FAILED;}
    
    // Check for minver < 3.60
    {
        u8 data[0x20];
        _memset(data, sizeof(data));
        GetApplicableVersion(data);
        if((data[1] >= 0x4) || (data[1] == 0x3 && data[3] >= 0x60))
        {
            BEEP3; cellFsClose(fd); ros_patching = false;
            ssend_message(conn_s, "<p color=\"color:red;\">Minver must be lower than 3.60! Yours is %x.%02x</p>", data[1], data[3]);
            return FAILED;
        }
    }

    // open flash
    const u32 flags = 0x01000000;
    u32 metldr_sec = 0x4, r; // metldr is always at sector 4, offset 0x20
    sys_device_handle_t flash_id;

    if(is_nor())
    {
        // Open NOR flash
        ssend_message(conn_s, "<p>Opening NOR...</p>");
        if(!usb_debug)
        {
            if(sys_storage_open(FLASH_DEVICE_NOR, 0, &flash_id, flags) != CELL_OK) {BEEP3; cellFsClose(fd); ros_patching = false; return FAILED;}
        }
        else
        {
            ssend_message(conn_s, "<p style=\"color:yellow;\">USB debug enabled</p>");
            if(sys_storage_open(0x10300000000000AULL, 0, &flash_id, flags) != CELL_OK) {BEEP3; cellFsClose(fd); ros_patching = false; return FAILED;}
        }
        ssend_message(conn_s, "<span style=\"color:green;\">OK</span>");
        ros_offset[0] = 0x10;
        ros_offset[1] = 0x10;
        ros_start_sector[0] = 0x600;
        ros_start_sector[1] = 0x3e00;
    }
    else
    {
        // Open NAND flash
        ssend_message(conn_s, "<p>Opening NAND...</p>");
        if(!usb_debug)
        {
            if(sys_storage_open(FLASH_DEVICE_NAND, 0, &flash_id, flags) != CELL_OK) {BEEP3; cellFsClose(fd); ros_patching = false; return FAILED;}
        }
        else
        {
            ssend_message(conn_s, "<p style=\"color:yellow;\">USB debug enabled</p>");
            if(sys_storage_open(0x10300000000000AULL, 0, &flash_id, flags) != CELL_OK) {BEEP3; cellFsClose(fd); ros_patching = false; return FAILED;}
        }
        ssend_message(conn_s, "<span style=\"color:green;\">OK</span>");
        ros_offset[0] = 0x30;
        ros_offset[1] = 0x20;
        ros_start_sector[0] = 0x400;
        ros_start_sector[1] = 0x3c00;
    }

    const int sec_size = 0x200; // 512 bytes

    char read_buf[sec_size + 0x30]; // length of one sector + something extra for more room to play
    memset(&read_buf, 0, sec_size + 0x30);

    char *item_name = ((char *) read_buf) + 0x20;

    // abort if metldr is not found at metldr_sec (or if metldr.2 is found)
    sys_storage_read(flash_id, 0, metldr_sec, 1, read_buf, &r, FLASH_FLAGS);
    if(!(IS(item_name, "metldr") && !IS(item_name, "metldr.2")))
    {
        ssend_message(conn_s, "<p style=\"color:red;\">Wrong metldr version!</p>");
        ret = FAILED; goto exit_ros;
    }

    // Prepare ROS overlays
    for(int cur_ros = 0; cur_ros <= 1; cur_ros++)
    {
        ssend_message(conn_s, "<p>Preparing ROS%d overlay...</p>", cur_ros);
        int fd_overlay;

        if(cellFsOpen(overlay_path[cur_ros], CELL_FS_O_CREAT|CELL_FS_O_WRONLY|CELL_FS_O_TRUNC, &fd_overlay, NULL, 0) != CELL_FS_SUCCEEDED)
        {
            ssend_message(conn_s, "<p style=\"color:red;\">Error creating overlay file!</p>");
            BEEP3; ros_patching = false; return FAILED;
        }

        //Write ROS header
        u64 n_written;
        memcpy(&read_buf, ROS_HEADER, sizeof(ROS_HEADER));
        
        // Special care for NAND based models
        if(!is_nor())
        {
            // Preserve first 16 bytes of ROS0
            char read_buf2[sec_size];

            sys_storage_read(flash_id, 0, ros_start_sector[0], 1, &read_buf2, &r, FLASH_FLAGS);
            if(r != 1)
            {
                ssend_message(conn_s, "<p style=\"color:red;\">Failed to read a sector from flash!</p>");
                BEEP3; ros_patching = false; return FAILED;
            }

            memcpy(&read_buf, &read_buf2, 16);

            if(cur_ros == 1)
            {
                // Zero fill 16 bytes at offset 0x10 in ROS1 header for NAND
                memset(&read_buf[0x10], 0x00, 16);
            }
        }

        // Write ROS header
        cellFsWrite(fd_overlay, &read_buf[sizeof(ROS_HEADER) - ros_offset[cur_ros]], ros_offset[cur_ros], &n_written);
        if(n_written != ros_offset[cur_ros])
        {
            ssend_message(conn_s, "<p style=\"color:red;\">Failed to write a ROS header!</p>");
            BEEP3; ros_patching = false; return FAILED;
        }

        //Copy noFSM patch
        u64 n_read;
        for(int sector = 0; sector <= 0x37FF; sector++)
        {
            // Read 1 sector
            if(cellFsReadWithOffset(fd, sector * sec_size, &read_buf, sec_size, &n_read) != CELL_FS_SUCCEEDED)
            {
                ssend_message(conn_s, "<p style=\"color:red;\">Failed to read from noFSM patch!</p>");
                BEEP3; ros_patching = false; return FAILED;
            }

            if(sector == 0x37FF)
            {
                if(ros_offset[cur_ros] < 0x20)
                {
                    // If we are at the very last sector of noFSM patch and the offset isn't 32 bytes or more.
                    // It means that the last noFSM read wasn't able to get a whole sector. (noFSM patch is always 32 bytes shorter)
                    // We are supposed to add padding so it's aligned exactly to 0x3800 sectors
                    char padding_val = is_nor() ? 0xFF: 0x00;

                    memset(&read_buf[n_read], padding_val, sec_size - n_read);
                    if(cellFsWrite(fd_overlay, read_buf, sec_size - (0x20 - ros_offset[cur_ros]), &n_written) != CELL_FS_SUCCEEDED)
                    {
                        ssend_message(conn_s, "<p style=\"color:red;\">Failed to prepare a noFSM patch!</p>");
                        BEEP3; ros_patching = false; return FAILED;
                    }
                }
                
                if(ros_offset[cur_ros] >= 0x20)
                {
                    // Write the last incomplete noFSM sector
                    // Write less if the offset is longer than 0x20
                    if(cellFsWrite(fd_overlay, read_buf, n_read - (ros_offset[cur_ros] - 0x20), &n_written) != CELL_FS_SUCCEEDED)
                    {
                        ssend_message(conn_s, "<p style=\"color:red;\">Failed to prepare a noFSM patch!</p>");
                        BEEP3; ros_patching = false; return FAILED;
                    }
                }
                
            }
            else
            {
                if(cellFsWrite(fd_overlay, read_buf, sec_size, &n_written) != CELL_FS_SUCCEEDED)
                {
                    ssend_message(conn_s, "<p style=\"color:red;\">Failed to prepare a noFSM patch!</p>");
                    BEEP3; ros_patching = false; return FAILED;
                }
            }

            if((sector % (0x37FF / 10)) == 0)
            {
                ssend_message(conn_s, "%d%%... ", (sector / (0x37FF / 10)) * 10);
            }
        }

        cellFsClose(fd_overlay);

        // Check that newly written overlay file is indeed 0x3800 sectors long, 7 340 032 bytes
        if(file_size(overlay_path[cur_ros]) != 7340032)
        {
            ssend_message(conn_s, "<span style=\"color:red;\">File size invalid!</span>");
            BEEP3; ros_patching = false; return FAILED;
        } else {
            ssend_message(conn_s, "<span style=\"color:green;\">OK</span>");
        }

        calc_md5(overlay_path[cur_ros], expected_ros_reg_hash[cur_ros]);
        ssend_message(conn_s, "<p>ROS%d overlay hash: %s</p>", cur_ros, expected_ros_reg_hash[cur_ros]);
    }

    // Check hashes before flashing
    ssend_message(conn_s, "<p>Calculating current ROS checksums...</p>");
    for(int cur_ros = 0; cur_ros <= 1; cur_ros++)
    {
        char region[32 + 1]; // Null terminated md5 hash
        char patch[32 + 1];
        ros_checksum(flash_id, ros_start_sector[cur_ros], ros_offset[cur_ros], region, patch);
        
        ssend_message(conn_s, "<p>ROS%d region hash: %s</p>", cur_ros, region);
        ssend_message(conn_s, "<p>ROS%d patch hash: %s</p>", cur_ros, patch);
    }

    /*
    
        ----! CRITICAL SECTION !----

    */

    // Write ROS overlays to flash
    for(int cur_ros = 0; cur_ros <= 1; cur_ros++)
    {
        ssend_message(conn_s, "<p>Writing ROS%d overlay to flash...</p>", cur_ros);
        int fd_overlay;

        if(cellFsOpen(overlay_path[cur_ros], CELL_FS_O_RDONLY, &fd_overlay, NULL, 0) != CELL_FS_SUCCEEDED) {BEEP3; ros_patching = false; return FAILED;}

        u64 n_read;
        u32 n_written;
        for(int sector = 0; sector <= 0x37FF; sector++)
        {
            if(cellFsReadWithOffset(fd_overlay, sector * sec_size, &read_buf, sec_size, &n_read) != CELL_FS_SUCCEEDED)
            {
                ssend_message(conn_s, "<p style=\"color:red;\">Failed to prepare a noFSM patch!</p>");
                BEEP3; ros_patching = false; return FAILED;
            }

            sys_storage_write(flash_id, ros_start_sector[cur_ros] + sector, 1, (void *) &read_buf, &n_written);
            if(n_written != 1)
            {
                ssend_message(conn_s, "<p style=\"color:red;\">Failed to write a noFSM patch!</p>");
                BEEP3; ros_patching = false; return FAILED;
            }

            if((sector % (0x37FF / 10)) == 0)
            {
                ssend_message(conn_s, "%d%%... ", (sector / (0x37FF / 10)) * 10);
            }
        }
        
        ssend_message(conn_s, "<span style=\"color:green;\">OK</span>");
        cellFsClose(fd_overlay);
    }

    /*
    
        ----! END OF CRITICAL SECTION !----

    */


    // Verify final ROS hashes.
    ssend_message(conn_s, "<p>Calculating ROS checksums...</p>");
    calc_md5(patch_file, expected_patch_hash);
    for(int cur_ros = 0; cur_ros <= 1; cur_ros++)
    {   
        char region[33]; // Null terminated md5 hash
        char patch[33];
        ros_checksum(flash_id, ros_start_sector[cur_ros], ros_offset[cur_ros], region, patch);

        if(IS(expected_ros_reg_hash[cur_ros], region))
        {
            ssend_message(conn_s, "<p>ROS%d region hash: <span style=\"color:green;\">%s OK!</span></p>", cur_ros, region);
        }
        else
        {
            ssend_message(conn_s, "<p>ROS%d region hash: <span style=\"color:red;\">%s INVALID HASH!</span></p>", cur_ros, region);
        }
        if(IS(expected_patch_hash, patch))
        {
            ssend_message(conn_s, "<p>ROS%d patch hash: <span style=\"color:green;\">%s OK!</span></p>", cur_ros, patch);
        }
        else
        {
            ssend_message(conn_s, "<p>ROS%d patch hash: <span style=\"color:red;\">%s INVALID HASH!</span></p>", cur_ros, patch);
        }
    }

exit_ros:
    // close & exit
    sys_storage_close(flash_id);
    cellFsClose(fd);

    if(ret) {BEEP3} else {BEEP2}

    stop_ros = ros_patching = false;
    return ret;
}
#endif //#ifdef PATCH_ROS