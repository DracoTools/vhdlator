-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity sub is
    generic (N : natural);
end entity;

architecture test of sub is

    -- The result is as long as the parameter, whose length names a generic
    function reverse (q : bit_vector) return bit_vector is
        variable r : bit_vector(q'range);
    begin
        for i in q'range loop
            r(q'high - i + q'low) := q(i);
        end loop;
        return r;
    end function;

    -- A parameter of the enclosing subprogram decides a length inside it
    procedure assign (v : out bit_vector; b : in bit) is
    begin
        v := (v'range => b);
    end procedure;

    procedure sized (x : natural) is
        variable v : bit_vector(1 to x);
    begin
        assign(v, '1');
        assert v = (1 to x => '1') report "sized" severity failure;
    end procedure;

    signal w : bit_vector(1 to N) := (others => '0');

begin

    process is
    begin
        w(1) <= '1';
        wait for 1 ns;
        assert reverse(w) = (1 to N - 1 => '0') & '1' report "reverse" severity failure;
        sized(12);
        sized(3);
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;

entity t_vhdl_specval is
end entity;

architecture test of t_vhdl_specval is
begin
    u : entity work.sub generic map (N => 5);
end architecture;
