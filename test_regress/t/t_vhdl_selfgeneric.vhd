-- DESCRIPTION: Verilator: VHDL unit that instantiates itself with a generic
-- written in terms of its own
--
-- "w => half(w)" names one generic on both sides of the association.  The
-- port's subtype is written with that generic, and standing the actual in
-- for it while the port is built would define it in terms of itself.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_selfgeneric_pkg is
    function half (v : bit_vector) return bit_vector;
end package;

package body t_vhdl_selfgeneric_pkg is
    function half (v : bit_vector) return bit_vector is
        variable r : bit_vector(0 to v'length / 2 - 1);
    begin
        for i in r'range loop
            r(i) := v(v'low + 2 * i);
        end loop;
        return r;
    end function;
end package body;

use work.t_vhdl_selfgeneric_pkg.all;

entity t_vhdl_selfgeneric_sub is
    generic (pow : natural;
             w : bit_vector(0 to 2 ** pow - 1));
    port (x : in bit_vector(0 to 2 ** pow - 1);
          y : out bit_vector(0 to 2 ** pow - 1));
end entity;

architecture rtl of t_vhdl_selfgeneric_sub is
begin
    stage_n : if pow > 0 generate
        signal lo : bit_vector(0 to w'length / 2 - 1);
    begin
        inner : entity work.t_vhdl_selfgeneric_sub
            generic map (pow => pow - 1, w => half(w))
            port map (x => half(x), y => lo);
        y(0 to w'length / 2 - 1) <= lo;
        y(w'length / 2 to w'length - 1) <= x(w'length / 2 to w'length - 1) xor w(w'length / 2 to w'length - 1);
    end generate;
    stage_0 : if pow = 0 generate
        y <= x xor w;
    end generate;
end architecture;

entity t_vhdl_selfgeneric is
end entity;

architecture test of t_vhdl_selfgeneric is
    signal x, y : bit_vector(0 to 3);
begin
    dut : entity work.t_vhdl_selfgeneric_sub
        generic map (pow => 2, w => "1010")
        port map (x => x, y => y);

    process is
    begin
        x <= "1101";
        wait for 1 ns;
        -- pow 2: w "1010", upper half x(2 to 3) xor w(2 to 3) = "01" xor "10"
        -- pow 1: x "10" (elements 0 and 2), w "11": upper "0" xor "1"
        -- pow 0: x "1", w "1": "0"
        assert y = "0111" report to_string(y) severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
