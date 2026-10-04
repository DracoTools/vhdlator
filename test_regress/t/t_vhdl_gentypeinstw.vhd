-- DESCRIPTION: Verilator: VHDL generic type sized by a generic of the
-- instantiating entity
--
-- "dat => dat" hands an entity's generic type a subtype whose length names
-- a generic of the instantiating entity: "subtype dat is
-- std_logic_vector(width - 1 downto 0)". The instantiated entity's ports, a
-- generic of that type, and a signal of an array of it all take the length
-- each instance of the instantiating entity has (GHDL gna issue2584,
-- issue2691).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_gentypeinstw_reg is
    generic (type dat; init : dat; stages : natural := 2);
    port (clk : in std_logic; i : in dat; o : out dat);
end entity;

architecture rtl of t_vhdl_gentypeinstw_reg is
    type chain_t is array (0 to stages - 1) of dat;
    signal chain : chain_t := (others => init);
begin
    o <= chain(stages - 1);
    process (clk) is
    begin
        if rising_edge(clk) then
            chain(0) <= i;
            for k in 0 to stages - 2 loop
                chain(k + 1) <= chain(k);
            end loop;
        end if;
    end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_gentypeinstw_pass is
    generic (type dat);
    port (i : in dat; o : out dat);
end entity;

architecture a of t_vhdl_gentypeinstw_pass is
begin
    o <= i;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_gentypeinstw_unit is
    generic (width : positive := 8; seed : std_logic_vector);
    port (clk : in std_logic; q : out std_logic_vector(width - 1 downto 0);
          len : out natural);
end entity;

architecture a of t_vhdl_gentypeinstw_unit is
    subtype dat is std_logic_vector(width - 1 downto 0);
    constant ones : dat := (others => '1');
    signal d, r : dat;
begin
    d <= seed;
    u_reg : entity work.t_vhdl_gentypeinstw_reg
        generic map (dat => dat, init => ones, stages => 3)
        port map (clk, d, r);
    u_pass : entity work.t_vhdl_gentypeinstw_pass
        generic map (dat => dat)
        port map (r, q);
    len <= q'length;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_gentypeinstw is
end entity;

architecture test of t_vhdl_gentypeinstw is
    signal clk : std_logic := '0';
    signal q4 : std_logic_vector(3 downto 0);
    signal q12 : std_logic_vector(11 downto 0);
    signal len4, len12 : natural;
begin
    u4 : entity work.t_vhdl_gentypeinstw_unit
        generic map (width => 4, seed => "1010")
        port map (clk, q4, len4);
    u12 : entity work.t_vhdl_gentypeinstw_unit
        generic map (width => 12, seed => x"5C3")
        port map (clk, q12, len12);

    process is
    begin
        wait for 1 ns;
        assert len4 = 4 and len12 = 12 severity failure;
        assert q4 = "1111" severity failure;
        assert q12 = x"FFF" severity failure;
        for k in 1 to 2 loop
            clk <= '1';
            wait for 1 ns;
            clk <= '0';
            wait for 1 ns;
        end loop;
        assert q4 = "1111" severity failure;
        assert q12 = x"FFF" severity failure;
        clk <= '1';
        wait for 1 ns;
        assert q4 = "1010" report "q4 " & to_string(q4) severity failure;
        assert q12 = x"5C3" report "q12 " & to_string(q12) severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
