-- DESCRIPTION: Verilator: VHDL table computed while elaborating
--
-- A generic given what a function returns, the function filling a table of
-- 32768 elements in a loop, and the instance reading it. Elaboration runs
-- such a function however long its loops are (14.4.2), so a loop from VHDL
-- source is not held to the limit for Verilog's (NVC issue571).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_elabtable_pkg is
    constant SIZE : natural := 32768;
    type table_t is array (natural range <>) of bit_vector(7 downto 0);
end package;

use work.t_vhdl_elabtable_pkg.all;

entity t_vhdl_elabtable_sub is
    generic (init : table_t);
end entity;

architecture a of t_vhdl_elabtable_sub is
begin
    process is
    begin
        assert init'length = SIZE severity failure;
        assert init(0) = x"03" severity failure;
        assert init(1) = x"0a" severity failure;
        assert init(SIZE - 1) = x"fc" severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;

use work.t_vhdl_elabtable_pkg.all;

entity t_vhdl_elabtable is
end entity;

architecture a of t_vhdl_elabtable is
    function make_table return table_t is
        variable r : table_t(0 to SIZE - 1);
        variable v : natural;
    begin
        for i in 0 to SIZE - 1 loop
            v := (i * 7 + 3) mod 256;
            for j in 0 to 7 loop
                if (v / 2 ** j) mod 2 = 1 then
                    r(i)(j) := '1';
                else
                    r(i)(j) := '0';
                end if;
            end loop;
        end loop;
        return r;
    end function;
begin
    u : entity work.t_vhdl_elabtable_sub generic map (init => make_table);
end architecture;
