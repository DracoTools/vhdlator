-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

entity t_vhdl_filerec is
    -- Where to put the files, given on the command line
    generic (OUTDIR : string := ".");
end entity;

architecture test of t_vhdl_filerec is
    type rec is record
        a : boolean;
        b : bit;
        c : character;
        e : integer;
        g : time;
    end record;
    type rec_file is file of rec;

    type int_vec is array (1 to 3) of integer;
    type pair is record
        name : character;
        vals : int_vec;
    end record;
    type pair_file is file of pair;
    type pair_vec is array (0 to 1) of pair;
    type pairs_file is file of pair_vec;

    constant REC_NAME  : string := OUTDIR & "/t_vhdl_filerec_rec.bin";
    constant PAIR_NAME : string := OUTDIR & "/t_vhdl_filerec_pair.bin";
    constant VEC_NAME  : string := OUTDIR & "/t_vhdl_filerec_vec.bin";
begin

    main : process is
        -- 5.5.2: the representation of a file of any type is the
        -- implementation's own; a design reading its own file back must get
        -- what it wrote
        file wf  : rec_file open write_mode is REC_NAME;
        file rf  : rec_file;
        file wp  : pair_file open write_mode is PAIR_NAME;
        file rp  : pair_file;
        file wv  : pairs_file open write_mode is VEC_NAME;
        file rv  : pairs_file;
        variable r1, r2 : rec;
        variable p1, p2 : pair;
        variable v1, v2 : pair_vec;
    begin
        r1 := (a => true, b => '1', c => 'Q', e => -42, g => 7 ns);
        write(wf, r1);
        write(wf, (a => false, b => '0', c => 'z', e => 99, g => 3 ps));
        file_close(wf);

        p1 := (name => 'x', vals => (10, 20, 30));
        write(wp, p1);
        file_close(wp);

        v1 := ((name => 'a', vals => (1, 2, 3)), (name => 'b', vals => (4, 5, 6)));
        write(wv, v1);
        file_close(wv);

        file_open(rf, REC_NAME, read_mode);
        read(rf, r2);
        assert r2 = r1 report "a record read back as it was written" severity failure;
        read(rf, r2);
        assert r2.c = 'z' and r2.e = 99 and r2.g = 3 ps and not r2.a
            report "and the one after it" severity failure;
        assert endfile(rf) report "and nothing more" severity failure;
        file_close(rf);

        file_open(rp, PAIR_NAME, read_mode);
        read(rp, p2);
        assert p2 = p1 report "a record holding an array" severity failure;
        file_close(rp);

        file_open(rv, VEC_NAME, read_mode);
        read(rv, v2);
        assert v2 = v1 report "an array of records" severity failure;
        file_close(rv);

        report "*-* All Finished *-*";
        std.env.stop;
    end process;
end architecture;
