-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A string held in a record, and strings held in an array, written to a file
-- and read back: each is its characters, as a file of STRING holds them.
-- Taken for one scalar the way a number is, the string was put into a
-- machine word, and the C++ did not compile (VESTS tc708).
entity t_vhdl_filestr is
    -- Where to put the files, given on the command line
    generic (OUTDIR : string := ".");
end entity;

architecture test of t_vhdl_filestr is
    subtype str8 is string (1 to 8);
    type entry is record
        number : natural;
        name   : str8;
        tag    : character;
    end record;
    type entry_file is file of entry;
    type names is array (1 to 3) of str8;
    type names_file is file of names;

    constant ENTRY_NAME : string := OUTDIR & "/t_vhdl_filestr_entry.bin";
    constant NAMES_NAME : string := OUTDIR & "/t_vhdl_filestr_names.bin";
begin
    main : process is
        file we : entry_file open write_mode is ENTRY_NAME;
        file re : entry_file;
        file wn : names_file open write_mode is NAMES_NAME;
        file rn : names_file;
        variable e1, e2 : entry;
        variable n1, n2 : names;
    begin
        e1 := (number => 7, name => "a string", tag => 'T');
        write(we, e1);
        write(we, (number => 9, name => "  blanks", tag => 'b'));
        file_close(we);

        n1 := ("first   ", "second  ", "third 3 ");
        write(wn, n1);
        file_close(wn);

        file_open(re, ENTRY_NAME, read_mode);
        read(re, e2);
        assert e2 = e1 report "a record holding a string" severity failure;
        read(re, e2);
        assert e2.number = 9 and e2.name = "  blanks" and e2.tag = 'b'
            report "and the one after it" severity failure;
        assert endfile(re) report "and nothing more" severity failure;
        file_close(re);

        file_open(rn, NAMES_NAME, read_mode);
        read(rn, n2);
        assert n2 = n1 report "an array of strings" severity failure;
        file_close(rn);

        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
