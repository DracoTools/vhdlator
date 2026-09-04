-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package retpk is
    type iv_t is array (integer range <>) of integer;
    -- One branch returns a single element, another returns what a further
    -- call gives back -- which is as long as that call makes it, not one
    function lineage (n : integer) return iv_t;
    function total (n : integer) return integer;
end package;

package body retpk is
    function build (n : integer) return iv_t is
        variable v : iv_t(1 to n);
    begin
        for i in 1 to n loop v(i) := i; end loop;
        return v;
    end function;

    function lineage (n : integer) return iv_t is
    begin
        if n <= 1 then
            return (1 => 7);
        end if;
        return build(n);
    end function;

    function total (n : integer) return integer is
        -- Takes its bounds from the value, which only running knows
        constant l : iv_t := lineage(n);
        variable s : integer := 0;
    begin
        for i in l'range loop s := s + l(i); end loop;
        return s + l'length * 100;
    end function;
end package body;

use work.retpk.all;

entity t_vhdl_retcall is
end entity;

architecture tb of t_vhdl_retcall is
begin
    process is
    begin
        assert total(1) = 107
            report "total(1) = " & integer'image(total(1)) severity failure;
        assert total(4) = 410
            report "total(4) = " & integer'image(total(4)) severity failure;
        assert total(5) = 515
            report "total(5) = " & integer'image(total(5)) severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
