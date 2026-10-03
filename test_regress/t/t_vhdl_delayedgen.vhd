-- DESCRIPTION: Verilator: VHDL S'DELAYED(T) with T a generic (16.2.4)
--
-- The time is a static expression, and a generic is as static as a literal:
-- each instance delays by what it was given, and zero is one delta back. A
-- transaction delayed by more than zero matures in the first delta of the
-- time it is due (14.7.5.3), so at 9 ps s'DELAYED(3 ps) is what s became
-- just after 6 ps.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_delayedgen_sub is
    generic ( tdelay : delay_length; expect, expect2 : natural );
end entity;

architecture test of t_vhdl_delayedgen_sub is
    signal s : natural;
begin

    gen: process is
    begin
        for i in 1 to 10 loop
            s <= s + 1;
            wait for 1 ps;
        end loop;
        wait;
    end process;

    check: process is
    begin
        wait for 9 ps;
        assert s = 9 severity failure;
        assert s'delayed(tdelay) = expect severity failure;
        wait for 0 ns;
        assert s = 10 severity failure;
        assert s'delayed(tdelay) = expect severity failure;
        assert s'delayed(2 * tdelay) = expect2 severity failure;
        wait;
    end process;

end architecture;

entity t_vhdl_delayedgen is
    generic ( tdelay : delay_length := 5 ps );
end entity;

architecture test of t_vhdl_delayedgen is
    signal s : natural;
begin

    sub3: entity work.t_vhdl_delayedgen_sub generic map ( tdelay => 3 ps, expect => 7, expect2 => 4 );
    sub0: entity work.t_vhdl_delayedgen_sub generic map ( tdelay => 0 ps, expect => 9, expect2 => 9 );

    gen: process is
    begin
        for i in 1 to 10 loop
            s <= s + 1;
            wait for 1 ps;
        end loop;
        wait;
    end process;

    process is
    begin
        wait for 9 ps;
        assert s'delayed(tdelay) = 5 severity failure;
        assert s'delayed = 9 severity failure;
        wait for 0 ns;
        assert s = 10 severity failure;
        assert s'delayed = 9 severity failure;
        assert s'delayed(tdelay) = 5 severity failure;
        wait for 1 ns;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
