-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

entity t_vhdl_nullarray is
end entity;

architecture test of t_vhdl_nullarray is
    type iv is array (integer range <>) of integer;
    subtype i4 is iv(1 to 4);
    subtype i1 is iv(1 to 1);
    subtype i0 is iv(1 to 0);
begin

    main : process is
        variable nul   : i0;
        variable four  : i4 := (1, 2, 3, 4);
        variable res4  : i4;
        variable res1  : i1;
        variable e     : integer := 7;
    begin
        -- 9.2.5: a null array contributes no elements to a concatenation
        assert nul'length = 0 report "a null subtype has no elements" severity failure;

        res4 := nul & four;
        assert res4 = (1, 2, 3, 4) report "null on the left" severity failure;

        res4 := four & nul;
        assert res4 = (1, 2, 3, 4) report "null on the right" severity failure;

        -- An element concatenated with a null array is still an array of one
        res1 := e & nul;
        assert res1(1) = 7 report "an element and a null array" severity failure;

        res1 := nul & e;
        assert res1(1) = 7 report "a null array and an element" severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
    end process;
end architecture;
