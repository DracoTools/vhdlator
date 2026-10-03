-- DESCRIPTION: Verilator: VHDL output port of a subtype resolved by a
-- function of the design's own, driven several times inside its entity
--
-- The driving value of an output is what its resolution function makes of
-- the drivers the architecture gives it (14.7.3.2): that is what the port
-- reads inside, and it reaches the actual as one source of it. Each
-- architecture of the entity counts its own drivers -- one drives the port
-- twice, one three times through a generate whose range an impure function
-- that reaches nothing outside itself gives (11.8) -- and one drives it not
-- at all, so its port is a source holding its default (14.7.3.2).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_resoutport_pkg is
    type int_vec is array (natural range <>) of integer;
    function sum (v : int_vec) return integer;
    subtype rint is sum integer;
    impure function get_limit return integer;
end package;

package body t_vhdl_resoutport_pkg is
    function sum (v : int_vec) return integer is
        variable r : integer := 0;
    begin
        for i in v'range loop
            r := r + v(i);
        end loop;
        return r;
    end function;

    impure function get_limit return integer is
        variable n : integer := 1;
    begin
        for i in 1 to 2 loop
            n := n + 1;
        end loop;
        return n;
    end function;
end package body;

-------------------------------------------------------------------------------

use work.t_vhdl_resoutport_pkg.all;

entity t_vhdl_resoutport_sub is
    port ( o : out rint := 0 );
end entity;

architecture two of t_vhdl_resoutport_sub is
begin
    o <= 5;
    o <= 7;
    process is
    begin
        wait for 1 ns;
        assert o = 12 report "two: " & integer'image(o) severity failure;
        wait;
    end process;
end architecture;

architecture three of t_vhdl_resoutport_sub is
    constant limit : integer := get_limit;
begin
    g : for i in 1 to limit generate
        o <= i;
    end generate;
    process is
    begin
        wait for 1 ns;
        assert o = 6 report "three: " & integer'image(o) severity failure;
        wait;
    end process;
end architecture;

architecture none of t_vhdl_resoutport_sub is
begin
end architecture;

-------------------------------------------------------------------------------

use work.t_vhdl_resoutport_pkg.all;

entity t_vhdl_resoutport is
end entity;

architecture test of t_vhdl_resoutport is
    signal s : rint := 0;
begin

    u1 : entity work.t_vhdl_resoutport_sub(two) port map (s);
    u2 : entity work.t_vhdl_resoutport_sub(three) port map (s);
    u3 : entity work.t_vhdl_resoutport_sub(none) port map (s);
    s <= 100;

    process is
    begin
        wait for 2 ns;
        -- 100, 12, 6, and the default of the port no driver gives a value
        assert s = 118 report "s: " & integer'image(s) severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
