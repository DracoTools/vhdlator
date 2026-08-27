-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

entity t_vhdl_filetype is
end entity;

architecture test of t_vhdl_filetype is
    -- 5.5.2: every file type declares READ and WRITE of its own, and they
    -- are not the ones std.textio declares for a LINE
    type char_file is file of character;
    type str_file is file of string;
    type int_file is file of integer;

    type nat_vector is array (natural range <>) of natural;
    type nat_file is file of nat_vector;
begin

    main : process is
        file fc : char_file;
        file fs : str_file;
        file fi : int_file;
        file fn : nat_file;
        variable c : character;
        variable s : string(1 to 3);
        variable i : integer;
        variable v : nat_vector(1 to 4);
        variable len : natural;
        variable status : file_open_status;
    begin
        file_open(fc, "obj_vlt/t_vhdl_filetype/t_vhdl_filetype_c.bin", WRITE_MODE);
        write(fc, 'x');
        write(fc, 'y');
        write(fc, 'z');
        file_close(fc);

        file_open(fc, "obj_vlt/t_vhdl_filetype/t_vhdl_filetype_c.bin", READ_MODE);
        read(fc, c);
        assert c = 'x' report "first character" severity failure;
        read(fc, c);
        assert c = 'y' report "second character" severity failure;
        read(fc, c);
        assert c = 'z' report "third character" severity failure;
        assert endfile(fc) report "at the end" severity failure;
        file_close(fc);

        -- A file of STRING round-trips its own writing. How the values sit
        -- in the file is up to the implementation (5.5.2), so only a file
        -- this design wrote itself is read back here.
        file_open(fs, "obj_vlt/t_vhdl_filetype/t_vhdl_filetype_s.bin", WRITE_MODE);
        write(fs, string'("xyz"));
        file_close(fs);

        file_open(fs, "obj_vlt/t_vhdl_filetype/t_vhdl_filetype_s.bin", READ_MODE);
        read(fs, s, len);
        assert len = 3 report "length read" severity failure;
        assert s = "xyz" report "string read" severity failure;
        file_close(fs);

        file_open(fi, "obj_vlt/t_vhdl_filetype/t_vhdl_filetype_i.bin", WRITE_MODE);
        write(fi, 42);
        write(fi, -7);
        file_close(fi);

        file_open(fi, "obj_vlt/t_vhdl_filetype/t_vhdl_filetype_i.bin", READ_MODE);
        read(fi, i);
        assert i = 42 report "first integer" severity failure;
        read(fi, i);
        assert i = -7 report "second integer" severity failure;
        file_close(fi);

        -- An array whose elements do not pack goes element by element, and
        -- the reader is told how many it got
        file_open(fn, "obj_vlt/t_vhdl_filetype/t_vhdl_filetype_n.bin", WRITE_MODE);
        write(fn, nat_vector'(3, 1, 4, 1));
        file_close(fn);

        file_open(fn, "obj_vlt/t_vhdl_filetype/t_vhdl_filetype_n.bin", READ_MODE);
        read(fn, v, len);
        assert len = 4 report "elements read" severity failure;
        assert v = (3, 1, 4, 1) report "array read back" severity failure;
        file_close(fn);

        -- 16.3: a file that will not open says so
        file_open(status, fc, "obj_vlt/t_vhdl_filetype/t_vhdl_filetype_absent.bin", READ_MODE);
        assert status = NAME_ERROR report "open of a file that is not there"
            severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
    end process;
end architecture;
