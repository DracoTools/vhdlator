-- DESCRIPTION: Verilator: VHDL generic type given an unconstrained array type
--
-- "tyi => bit_vector" gives a generic type an unconstrained array type, so
-- each port of that type takes its subtype from its actual (5.3.2.2,
-- 6.5.6.3) as a port written "bit_vector" would. Two instances connect
-- vectors of different lengths and directions to the one copy; another
-- gives the generic type INTEGER, and a generic function is applied to
-- values of it (GHDL gna issue2702).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_gentypeuncon_gen is
    generic (
        type tyi;
        function mux (a : tyi; b : tyi; en : boolean) return tyi
    );
    port (i1 : in tyi; i2 : in tyi; en : in boolean; po : out tyi);
end entity;

architecture bhv of t_vhdl_gentypeuncon_gen is
begin
    po <= mux(i1, i2, en);
end architecture;

entity t_vhdl_gentypeuncon_vec is
    generic (type tyi);
    port (i : in tyi; o : out tyi);
end entity;

architecture bhv of t_vhdl_gentypeuncon_vec is
begin
    o <= i;
end architecture;

entity t_vhdl_gentypeuncon is
end entity;

architecture test of t_vhdl_gentypeuncon is
    function bmux (a : bit_vector; b : bit_vector; en : boolean) return bit_vector is
    begin
        if en then
            return a;
        else
            return b;
        end if;
    end function;
    function imux (a : integer; b : integer; en : boolean) return integer is
    begin
        if en then
            return a + b;
        else
            return 0;
        end if;
    end function;

    signal s8a : bit_vector(7 downto 0) := "00111100";
    signal s8b : bit_vector(7 downto 0) := "01010101";
    signal r8 : bit_vector(7 downto 0);
    signal s4a : bit_vector(1 to 4) := "1100";
    signal s4b : bit_vector(1 to 4) := "0011";
    signal r4 : bit_vector(1 to 4);
    signal ia : integer := 5;
    signal ib : integer := 4;
    signal ir : integer;
    signal sel : boolean := true;
    signal v12 : bit_vector(11 downto 0) := x"A5C";
    signal w12 : bit_vector(11 downto 0);
    signal v3 : bit_vector(2 to 4) := "101";
    signal w3 : bit_vector(2 to 4);
begin
    u8 : entity work.t_vhdl_gentypeuncon_gen
        generic map (tyi => bit_vector, mux => bmux)
        port map (s8a, s8b, sel, r8);
    u4 : entity work.t_vhdl_gentypeuncon_gen
        generic map (tyi => bit_vector, mux => bmux)
        port map (i1 => s4a, i2 => s4b, en => sel, po => r4);
    ui : entity work.t_vhdl_gentypeuncon_gen
        generic map (tyi => integer, mux => imux)
        port map (ia, ib, sel, ir);
    uv : entity work.t_vhdl_gentypeuncon_vec
        generic map (tyi => bit_vector)
        port map (v12, w12);
    uv3 : entity work.t_vhdl_gentypeuncon_vec
        generic map (tyi => bit_vector)
        port map (v3, w3);

    process is
    begin
        wait for 1 ns;
        assert r8 = "00111100" report "r8" severity failure;
        assert r4 = "1100" report "r4" severity failure;
        assert ir = 9 report "ir" severity failure;
        assert w12 = x"A5C" report "w12" severity failure;
        assert w3 = "101" report "w3" severity failure;
        sel <= false;
        wait for 1 ns;
        assert r8 = "01010101" report "r8 b" severity failure;
        assert r4 = "0011" report "r4 b" severity failure;
        assert ir = 0 report "ir b" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
