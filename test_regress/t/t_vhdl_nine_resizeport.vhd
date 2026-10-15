-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Under --vhdl-metavalues, numeric_std's RESIZE of a port whose bounds the
-- instance gives, to the length of another such port: neither length is a
-- number until elaboration, and RESIZE is emitted natively at whatever
-- length that settles. An UNSIGNED is filled with '0' or cut from the left;
-- a SIGNED is filled with its sign element, or cut keeping its sign
-- leftmost -- metavalues and all (gna issue961); and to a length of zero,
-- a null array, as in a copy of numeric_std's DIVMOD (VUnit tb_string_ops)

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_nine_resizeport_sub is
  generic (nullw : natural := 0);
  port (ui : in unsigned; uo : out unsigned; si : in signed; so : out signed);
end entity;

architecture a of t_vhdl_nine_resizeport_sub is
begin
  uo <= resize(ui, uo'length);
  so <= resize(si, so'length);

  process (ui)
    variable vnull : unsigned(nullw - 1 downto 0);
  begin
    vnull := resize(ui, nullw);
    assert vnull'length = 0 severity failure;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_nine_resizeport is
end entity;

architecture a of t_vhdl_nine_resizeport is
  signal ui : unsigned(7 downto 0) := "1X0Z1LH-";
  signal si : signed(5 downto 0) := "W01X1H";
  signal sn : signed(5 downto 0) := "1001X0";
  signal uw : unsigned(11 downto 0);
  signal un : unsigned(3 downto 0);
  signal ue : unsigned(7 downto 0);
  signal sw : signed(9 downto 0);
  signal snw : signed(9 downto 0);
  signal snn : signed(3 downto 0);
  signal sn1 : signed(0 downto 0);
  signal dummy_u : unsigned(7 downto 0);
begin
  w : entity work.t_vhdl_nine_resizeport_sub port map (ui, uw, si, sw);
  n : entity work.t_vhdl_nine_resizeport_sub port map (ui, un, sn, snn);
  e : entity work.t_vhdl_nine_resizeport_sub port map (ui, ue, sn, snw);
  o : entity work.t_vhdl_nine_resizeport_sub port map (ui, dummy_u, sn, sn1);

  process
  begin
    wait for 1 ns;
    report to_string(uw) & " " & to_string(un) & " " & to_string(ue);
    report to_string(sw) & " " & to_string(snw) & " " & to_string(snn) & " "
      & to_string(sn1);
    assert to_string(uw) = "00001X0Z1LH-" severity failure;
    assert to_string(un) = "1LH-" severity failure;
    assert to_string(ue) = "1X0Z1LH-" severity failure;
    assert to_string(sw) = "WWWWW01X1H" severity failure;
    assert to_string(snw) = "11111001X0" severity failure;
    assert to_string(snn) = "11X0" severity failure;
    assert to_string(sn1) = "1" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
