-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_dynop is
end entity;

architecture test of t_vhdl_dynop is

    -- An operator a design writes for itself is an ordinary subprogram, and
    -- its operands may be values whose length only running decides
    function "srl" (l : bit_vector; n : integer) return bit_vector is
        variable r : bit_vector(l'range) := (others => '0');
    begin
        for i in l'low to l'high - n loop
            r(i) := l(i + n);
        end loop;
        return r;
    end function;

    function "xnor" (l, r : bit_vector) return boolean is
    begin
        return l'length = r'length;
    end function;

    -- Returns of differing lengths, so what comes back carries its bounds
    function pick (n : natural; v : bit_vector) return bit_vector is
    begin
        if n = 0 then
            return v(3 downto 0);
        end if;
        return v;
    end function;

    signal d : natural := 1;

begin

    main : process is
        variable v : bit_vector(7 downto 0) := "10110010";
    begin
        assert (pick(d, v) srl 1) = "01011001" report "srl" severity failure;
        assert (pick(0, v) srl 1) = "0001" report "srl short" severity failure;
        assert (pick(d, v) xnor v) report "same length" severity failure;
        assert not (pick(0, v) xnor v) report "other length" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
