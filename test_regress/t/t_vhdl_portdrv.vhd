-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package pack is
    type nat_vector is array (natural range <>) of natural;
    function sum_all (arg : nat_vector) return natural;
    subtype summed is sum_all natural;
end package;

package body pack is
    function sum_all (arg : nat_vector) return natural is
        variable total : natural := 0;
    begin
        for i in arg'range loop
            total := total + arg(i);
        end loop;
        return total;
    end function;
end package body;

use work.pack.all;

entity contributor is
    generic ( amount : natural );
    port ( o : out natural );
end entity;

architecture rtl of contributor is
begin
    o <= amount;
end architecture;

use work.pack.all;

entity t_vhdl_portdrv is
end entity;

architecture test of t_vhdl_portdrv is
    signal total : summed;
begin
    -- Three drivers of one resolved signal: two ports and a process
    u1 : entity work.contributor generic map ( amount => 1 ) port map ( o => total );

    inner : block
    begin
        u2 : entity work.contributor generic map ( amount => 20 ) port map ( o => total );
    end block;

    third : process is
    begin
        total <= 300;
        wait;
    end process;

    check : process is
    begin
        wait for 1 ns;
        assert total = 321 report "total = " & integer'image(total) severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
