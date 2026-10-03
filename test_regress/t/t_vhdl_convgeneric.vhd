-- DESCRIPTION: Verilator: VHDL conversions on formals whose subtype names
-- the instance's generic (6.5.7.1)
--
-- "sum_ints(o1) => x1": the conversion is applied to a port whose bounds
-- the instance's own generic gives, so its copy is made where the instance
-- binds that generic. Inside, a process variable of the same subtype starts
-- at its default and is copied whole into the port.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_convgeneric_pkg is
    subtype pos is integer range 1 to 1000;
    type pos_vector is array (natural range <>) of pos;
end package;

use work.t_vhdl_convgeneric_pkg.all;

entity t_vhdl_convgeneric_sub is
    generic ( size : natural );
    port ( o1 : out pos_vector(1 to size);
           o2 : out pos_vector(1 to 3) );
end entity;

architecture test of t_vhdl_convgeneric_sub is
begin
    p1: process is
        variable tmp : pos_vector(1 to size);
    begin
        -- Not given one, a variable starts at POS'LEFT in every element
        assert tmp(1) = 1 severity failure;
        assert tmp(size) = 1 severity failure;
        for i in 1 to size loop
            tmp(i) := i;
        end loop;
        o1 <= tmp;
        o2 <= (5, 6, 7);
        wait for 1 ns;
        tmp(1) := 10;
        o1 <= tmp;
        wait;
    end process;
end architecture;

use work.t_vhdl_convgeneric_pkg.all;

entity t_vhdl_convgeneric is
end entity;

architecture test of t_vhdl_convgeneric is
    signal x1, x2, x3 : integer;
    signal y : pos_vector(1 to 4);

    function sum_ints(v : in pos_vector) return integer is
        variable result : integer := 0;
    begin
        for i in v'range loop
            result := result + v(i);
        end loop;
        return result;
    end function;
begin

    uut1: entity work.t_vhdl_convgeneric_sub
        generic map ( size => 5 )
        port map ( sum_ints(o1) => x1, sum_ints(o2) => x2 );

    uut2: entity work.t_vhdl_convgeneric_sub
        generic map ( size => 4 )
        port map ( o1 => y, sum_ints(o2) => x3 );

    process is
    begin
        wait for 500 ps;
        assert x1 = 15 severity failure;
        assert x2 = 18 severity failure;
        assert y = (1, 2, 3, 4) severity failure;
        wait for 1 ns;
        assert x1 = 24 severity failure;
        assert y = (10, 2, 3, 4) severity failure;
        assert x3 = 18 severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
