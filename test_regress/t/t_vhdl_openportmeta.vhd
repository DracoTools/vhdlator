-- DESCRIPTION: Verilator: VHDL input left open, compared with its metavalue default
--
-- This file is part of Verilator.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
--
-- An input the instance leaves open holds its default for as long as the
-- design runs. Where the default is written with a metavalue the two-state
-- runtime reads it as zeros, so "strb = (strb'range => 'U')" could not tell
-- an open strb from one driven all zero -- which is how VUnit's
-- resolve_tstrb tells whether tstrb is connected. Whether the port is open
-- is known when the design is elaborated, and answers the comparison.

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_openportmeta_sub is
  port (
    keep : in std_logic_vector(3 downto 0);
    strb : in std_logic_vector(3 downto 0) := (others => 'U');
    en   : in std_logic := 'U';
    res  : out std_logic_vector(3 downto 0);
    open_en : out boolean);
end entity;
architecture a of t_vhdl_openportmeta_sub is
  function resolve(keep, strb : std_logic_vector) return std_logic_vector is
  begin
    if strb = (strb'range => 'U') then
      return keep;
    else
      return strb;
    end if;
  end;
begin
  res <= resolve(keep, strb) when strb = (strb'range => 'U') else strb;
  open_en <= en = 'U';
end architecture;

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_openportmeta is
end entity;
architecture a of t_vhdl_openportmeta is
  signal keep : std_logic_vector(3 downto 0) := "1111";
  signal zeros : std_logic_vector(3 downto 0) := "0000";
  signal r1, r2 : std_logic_vector(3 downto 0);
  signal e1, e2 : boolean;
  signal one : std_logic := '1';
begin
  c1 : entity work.t_vhdl_openportmeta_sub port map (keep => keep, strb => zeros, en => one, res => r1, open_en => e1);
  c2 : entity work.t_vhdl_openportmeta_sub port map (keep => keep, res => r2, open_en => e2);
  process
  begin
    wait for 1 ns;
    report "connected " & to_string(r1) & " " & boolean'image(e1) & " open " & to_string(r2) & " " & boolean'image(e2);
    assert r1 = "0000" report "connected strb" severity failure;
    assert r2 = "1111" report "open strb" severity failure;
    assert not e1 and e2 report "scalar" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
