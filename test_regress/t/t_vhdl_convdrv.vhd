-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_convdrv is
end entity;

architecture test of t_vhdl_convdrv is
    function ones (v : bit_vector) return natural is
        variable sum : natural := 0;
    begin
        for i in v'range loop
            if v(i) = '1' then sum := sum + 1; end if;
        end loop;
        return sum;
    end function;

    type int_vector is array (natural range <>) of integer;
    function resolve_sum (vs : int_vector) return integer is
        variable sum : integer := 0;
    begin
        for i in vs'range loop sum := sum + vs(i); end loop;
        return sum;
    end function;
    subtype rint is resolve_sum integer;

    signal s : integer := 0;
    signal n : natural := 0;
    signal r : rint := 0;
begin

    -- With a conversion on each side of the association the port holds what
    -- the one on the actual makes of the actual, not what its own driver
    -- was given: the value goes out rounded and comes back rounded
    -- (6.5.7.1).
    b1 : block is
        port (p : inout real := 0.0);
        port map (integer(p) => real(s));
    begin
        inner : process is
        begin
            p <= 1.5;
            wait for 0 ns;
            assert p = 2.0 report "p rounded: " & real'image(p) severity failure;
            p <= 4.4;
            wait for 0 ns;
            assert p = 4.0 report "p rounded: " & real'image(p) severity failure;
            wait;
        end process;
    end block;

    -- The conversion runs whenever the port's driver delivers, so a
    -- transaction that leaves the value alone is still a transaction of
    -- what the association drives (14.7.3.4)
    b2 : block is
        port (q : out bit_vector(7 downto 0));
        port map (ones(q) => n);
    begin
        stim : process is
        begin
            q <= X"03" after 1 ns;
            wait for 2 ns;
            q(0) <= '1';  -- Already '1': a transaction, not an event
            wait;
        end process;
    end block;

    -- Two blocks driving one signal through conversions of their own are
    -- two drivers of it, resolved together, and not two values written one
    -- over the other (6.5.6.3, 14.7.2)
    b3 : block is
        port (u : out real := 0.0);
        port map (integer(u) => r);
    begin
        u <= 2.4 after 1 ns;
    end block;

    b4 : block is
        port (w : out real := 0.0);
        port map (integer(w) => r);
    begin
        w <= 10.6 after 1 ns;
    end block;

    chk : process is
    begin
        wait for 0 ns;
        assert s = 2 report "s at 0: " & integer'image(s) severity failure;
        assert r = 0 report "r at 0: " & integer'image(r) severity failure;

        wait for 1 ns;
        assert r = 13 report "r at 1 ns: " & integer'image(r) severity failure;
        assert n = 2 report "n at 1 ns: " & integer'image(n) severity failure;
        assert n'active report "n idle at 1 ns" severity failure;
        assert n'last_active = 0 ns report "n last_active at 1 ns" severity failure;

        wait for 1 ns;
        assert not n'active report "n active at 2 ns" severity failure;
        assert n'last_active = 1 ns report "n last_active at 2 ns" severity failure;

        wait for 0 ns;
        assert n'active report "n idle after q(0)" severity failure;
        assert n = 2 report "n after q(0): " & integer'image(n) severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
