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

entity t_vhdl_geniter is
end entity;

architecture test of t_vhdl_geniter is
    signal total : summed;
begin
    -- Four drivers from one generate, one from each iteration
    g : for k in 1 to 4 generate
        u : entity work.contributor generic map ( amount => k * 10 ) port map ( o => total );
    end generate;

    -- And one written out
    d : for k in 3 downto 2 generate
        total <= k;
    end generate;

    check : process is
    begin
        wait for 1 ns;
        -- 10 + 20 + 30 + 40 + 3 + 2
        assert total = 105 report "total = " & integer'image(total) severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
