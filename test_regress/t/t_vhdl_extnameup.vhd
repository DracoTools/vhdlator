-- DESCRIPTION: Verilator: VHDL external name with a relative pathname starting one region up
--
-- "^" is the region that encloses this one (8.7): written in an
-- architecture, the one that instantiates it.  What follows names an object
-- of that region, or an instance in it and an object of that.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_extnameup_sub1 is
end entity;

architecture test of t_vhdl_extnameup_sub1 is
    signal t : natural;
    signal seen : natural := 0;
begin

    p1 : process is
        alias s is << signal ^.s : natural >>;
    begin
        s <= 5;
        wait for 0 ns;
        assert t = 42 severity failure;
        wait for 1 ns;
        -- Read as well as driven
        assert << signal ^.top_only : natural >> = 7 severity failure;
        seen <= << signal ^.top_only : natural >>;
        wait;
    end process;

end architecture;

entity t_vhdl_extnameup_sub2 is
end entity;

architecture test of t_vhdl_extnameup_sub2 is
begin

    p2 : process is
        alias t is << signal ^.u1.t : natural >>;
    begin
        t <= 42;
        wait;
    end process;

end architecture;

entity t_vhdl_extnameup is
end entity;

architecture test of t_vhdl_extnameup is
    signal s : natural;
    signal top_only : natural := 7;

    component t_vhdl_extnameup_sub1 is
    end component;

    component t_vhdl_extnameup_sub2 is
    end component;
begin

    u1 : component t_vhdl_extnameup_sub1;
    u2 : component t_vhdl_extnameup_sub2;

    p3 : process is
    begin
        assert s = 0 severity failure;
        wait for 0 ns;
        assert s = 5 severity failure;
        wait for 2 ns;
        assert << signal u1.seen : natural >> = 7 severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
