-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A slice named by another object's 'RANGE, "DASH(Dest'range)", is as long
-- as that range (8.5), the object's subtype sized by a generic. OSVVM's
-- AxiStream SetOptions tests build a transaction's parameter as
-- "Dash(ID'range) & Dash(Dest'range) & (User + 5) & "-"", DASH as long as
-- the widest of the three.

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_sliceobjrange_ctl is
  generic (ID_LEN : integer; DEST_LEN : integer; USER_LEN : integer);
end entity;
architecture a of t_vhdl_sliceobjrange_ctl is
  constant MAX_LEN : integer := maximum(maximum(ID_LEN, DEST_LEN), USER_LEN);
  constant DASH : std_logic_vector(MAX_LEN - 1 downto 0) := (others => '-');
  signal sdest : std_logic_vector(DEST_LEN - 1 downto 0);
begin
  process
    variable ID : std_logic_vector(ID_LEN - 1 downto 0);
    variable Dest : std_logic_vector(DEST_LEN - 1 downto 0);
    variable o : std_logic_vector(DEST_LEN - 1 downto 0);
    variable cat : std_logic_vector(DEST_LEN downto 0);
  begin
    assert to_string(DASH(Dest'range)) = "----"
      report "dest " & to_string(DASH(Dest'range)) severity failure;
    assert to_string(DASH(ID'range) & DASH(Dest'range)) = "------------"
      report "id & dest " & to_string(DASH(ID'range) & DASH(Dest'range)) severity failure;
    assert to_string("1" & DASH(sdest'range)) = "1----"
      report "signal " & to_string("1" & DASH(sdest'range)) severity failure;
    -- As long as the variable it is assigned to, which an assignment checks
    cat := DASH(Dest'range) & "1";
    assert to_string(cat) = "----1" report "cat " & to_string(cat) severity failure;
    o := DASH(Dest'range);
    assert to_string(o) = "----" report "o " & to_string(o) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_sliceobjrange is
end entity;
architecture a of t_vhdl_sliceobjrange is
  signal tid : std_logic_vector(7 downto 0);
  signal tdest, tuser : std_logic_vector(3 downto 0);
begin
  u : entity work.t_vhdl_sliceobjrange_ctl
    generic map (ID_LEN => tid'length, DEST_LEN => tdest'length, USER_LEN => tuser'length);
end architecture;
