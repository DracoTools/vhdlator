-- DESCRIPTION: Verilator: VHDL slice whose range is that of a call's result
--
-- "big(window(k)'range)" with window's result "bit_vector(k * 8 + 7 downto
-- k * 8)" and k a loop parameter: the result carries its bounds, which its
-- subtype writes in terms of the copy's own parameter, so they are read from
-- the call (nvc range1, which steps a window through a word this way).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_sliceofcall is
end entity;

architecture test of t_vhdl_sliceofcall is
    function window (k : natural) return bit_vector is
        variable r : bit_vector(k * 8 + 7 downto k * 8);
    begin
        return r;
    end function;

    signal big : bit_vector(31 downto 0) := x"12345678";
    signal part : bit_vector(7 downto 0);
begin
    process is
        type bytes_t is array (0 to 3) of bit_vector(7 downto 0);
        constant want : bytes_t := (x"78", x"56", x"34", x"12");
        variable v : bit_vector(7 downto 0);
    begin
        for k in 0 to 3 loop
            part <= big(window(k)'range);
            v := big(window(k)'range);
            wait for 1 ns;
            assert part = want(k) severity failure;
            assert v = want(k) severity failure;
        end loop;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
