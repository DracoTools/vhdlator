-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- Two procedures that drive an inout port through a signal parameter, called
-- one after the other by one process, drive it with one driver (14.7.2): the
-- second's writes replace the first's. UVVM's AXI-Stream VVC receives after
-- an expect; each is a copy of a bus-functional model, and the value a copy
-- left the port's driver holding was ORed into every write of the next, so
-- the receiver could not lower tready. What made the driver a net of its own
-- per copy was a 'Z' written into it, as a slave writes its outputs.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
package t_vhdl_subdrvcopies_pkg is
  type t_if is record
    tvalid : std_logic;
    tready : std_logic;
    tdata  : std_logic_vector(7 downto 0);
  end record;
  function init_if(is_master : boolean; rdy : std_logic) return t_if;
  procedure tx(signal clk : in std_logic; signal i : inout t_if; constant n : in natural);
  procedure rx(signal clk : in std_logic; signal i : inout t_if; constant n : in natural;
               constant low_at : in natural; variable got : out natural);
  procedure rx2(signal clk : in std_logic; signal i : inout t_if; constant n : in natural;
               constant low_at : in natural; variable got : out natural);
end package;
package body t_vhdl_subdrvcopies_pkg is
  function init_if(is_master : boolean; rdy : std_logic) return t_if is
    variable v : t_if;
  begin
    if is_master then
      v := (tvalid => '0', tready => 'Z', tdata => x"00");
    else
      v := (tvalid => 'Z', tready => rdy, tdata => (others => 'Z'));
    end if;
    return v;
  end function;
  procedure tx(signal clk : in std_logic; signal i : inout t_if; constant n : in natural) is
    variable r : std_logic;
  begin
    for k in 1 to n loop
      i.tvalid <= '1';
      i.tdata <= std_logic_vector(to_unsigned(k, 8));
      wait until rising_edge(clk);
      r := i.tready;
      while r = '0' loop
        wait until rising_edge(clk);
        r := i.tready;
      end loop;
      i <= init_if(true, '0');
    end loop;
  end procedure;
  procedure rx(signal clk : in std_logic; signal i : inout t_if; constant n : in natural;
               constant low_at : in natural; variable got : out natural) is
    variable cnt : natural := 0;
  begin
    i <= init_if(false, '1');
    i.tvalid <= 'Z';
    i.tdata <= (others => 'Z');
    while cnt < n loop
      if cnt = low_at then
        i.tready <= '0';
        wait until rising_edge(clk);
        wait until rising_edge(clk);
      end if;
      i.tready <= '1';
      wait until rising_edge(clk);
      if i.tvalid = '1' and i.tready = '1' then
        cnt := cnt + 1;
      end if;
    end loop;
    got := cnt;
  end procedure;
  procedure rx2(signal clk : in std_logic; signal i : inout t_if; constant n : in natural;
               constant low_at : in natural; variable got : out natural) is
    variable cnt : natural := 0;
  begin
    i <= init_if(false, '1');
    i.tvalid <= 'Z';
    i.tdata <= (others => 'Z');
    while cnt < n loop
      if cnt = low_at then
        i.tready <= '0';
        wait until rising_edge(clk);
        wait until rising_edge(clk);
      end if;
      i.tready <= '1';
      wait until rising_edge(clk);
      if i.tvalid = '1' and i.tready = '1' then
        cnt := cnt + 1;
      end if;
    end loop;
    got := cnt;
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_subdrvcopies_pkg.all;
entity t_vhdl_subdrvcopies_vvc is
  generic (IS_MASTER : boolean; RDY : std_logic := '1');
  port (clk : in std_logic; i : inout t_if := init_if(IS_MASTER, RDY));
end entity;
architecture a of t_vhdl_subdrvcopies_vvc is
begin
  process
    variable got : natural;
  begin
    wait for 12 ns;
    if IS_MASTER then
      tx(clk, i, 8);
      wait for 20 ns;
      tx(clk, i, 8);
    else
      rx(clk, i, 8, 6, got);
      assert got = 8 report "first packet " & integer'image(got) severity failure;
 rx2(clk, i, 8, 3, got);
      assert got = 8 report "second packet " & integer'image(got) severity failure;
    end if;
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_subdrvcopies_pkg.all;
entity t_vhdl_subdrvcopies is
end entity;
architecture a of t_vhdl_subdrvcopies is
  signal clk : std_logic := '0';
  signal b : t_if;
  signal stalls : natural := 0;
begin
  clk <= not clk after 5 ns;
  m : entity work.t_vhdl_subdrvcopies_vvc generic map (IS_MASTER => true) port map (clk => clk, i => b);
  s : entity work.t_vhdl_subdrvcopies_vvc generic map (IS_MASTER => false) port map (clk => clk, i => b);
  -- Ready held low for two cycles with valid high, in each packet
  process (clk)
  begin
    if rising_edge(clk) and b.tvalid = '1' and b.tready = '0' then
      stalls <= stalls + 1;
    end if;
  end process;
  process
  begin
    wait for 400 ns;
    assert stalls = 4 report "ready was low " & integer'image(stalls) & " times" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
