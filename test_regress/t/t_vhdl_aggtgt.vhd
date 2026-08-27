-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_aggtgt is
end entity;

architecture test of t_vhdl_aggtgt is

    type inner_t is record
        f : bit;
        g : bit_vector(1 to 3);
    end record;

    type outer_t is record
        i : inner_t;
        h : bit;
    end record;

    signal src : bit_vector(1 downto 0) := "10";
    signal s0, s1 : bit;

begin

    -- A concurrent aggregate target
    conc : (s0, s1) <= src;

    process is
        variable a, h : bit;
        variable b : bit_vector(1 to 3);
        variable p, q, r : bit;
        variable ov : outer_t;
        variable text : string(1 to 8) := "abcdefgh";
        variable ch1, ch2 : character;
        variable rest : string(1 to 6);
    begin
        ov := (('1', "010"), '0');

        -- A nested aggregate target, over a record inside a record
        ((a, b), h) := ov;
        assert a = '1' report "a" severity failure;
        assert b = "010" report "b" severity failure;
        assert h = '0' report "h" severity failure;

        -- A nested aggregate target, over an array inside a record
        ((a, (p, q, r)), h) := ov;
        assert p = '0' and q = '1' and r = '0' report "pqr" severity failure;

        -- A named aggregate target, over a string. GHDL 7.0.0-dev refuses a
        -- discrete range as the choice of an aggregate target; nvc 1.22.1
        -- takes it, which is what VHDL-2008 asks for.
        (1 => ch1, 2 => ch2, 3 to 8 => rest) := text;
        assert ch1 = 'a' report "ch1" severity failure;
        assert ch2 = 'b' report "ch2" severity failure;
        assert rest = "cdefgh" report "rest" severity failure;

        -- A record aggregate target naming its elements out of order. nvc
        -- 1.22.1 stops with an internal error on this one; GHDL 7.0.0-dev
        -- gives the answer below.
        (h => h, i => (a, b)) := ov;
        assert a = '1' and b = "010" and h = '0' report "named record" severity failure;

        wait for 1 ns;
        assert s0 = '1' report "s0" severity failure;
        assert s1 = '0' report "s1" severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
