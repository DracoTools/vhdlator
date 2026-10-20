-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- Under --vhdl-metavalues, a record of elements each RESOLVED declared in a
-- package instance, as OSVVM's Wishbone harness declares its WishboneBus,
-- and driven by two instances: one through out ports given aliases of its
-- elements, one through an inout port of the whole record. Each wrote the
-- whole signal, so the two overwrote each other without end at the first
-- transfer, and time stopped.

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_nine_pkgrec_pkg is
  type wb_t is record
    Cyc : std_logic;
    Stb : std_logic;
    Ack : std_logic;
    Adr : std_logic_vector;
  end record;
  procedure InitRec(signal b : inout wb_t);
end package;
package body t_vhdl_nine_pkgrec_pkg is
  procedure InitRec(signal b : inout wb_t) is
  begin
    b <= (Cyc => 'Z', Stb => 'Z', Ack => 'Z', Adr => (b.Adr'range => 'Z'));
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_nine_pkgrec_pkg.all;
package t_vhdl_nine_pkgrec_sigs is
  generic (W : natural);
  signal WishboneBus : wb_t(Adr(W - 1 downto 0));
  alias Cyc is WishboneBus.Cyc;
  alias Stb is WishboneBus.Stb;
  alias Ack is WishboneBus.Ack;
  alias Adr is WishboneBus.Adr;
end package;

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_nine_pkgrec_dut is
  port (cyc_o : out std_logic; stb_o : out std_logic; adr_o : out std_logic_vector(7 downto 0);
        ack_i : in std_logic);
end;
architecture a of t_vhdl_nine_pkgrec_dut is
begin
  process
  begin
    cyc_o <= '0'; stb_o <= '0'; adr_o <= x"00";
    wait for 10 ns;
    cyc_o <= '1'; stb_o <= '1'; adr_o <= x"A5";
    wait until ack_i = '1' for 50 ns;
    report "dut sees ack=" & to_string(ack_i) & " at " & time'image(now);
    assert ack_i = '1' and now = 12 ns severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_nine_pkgrec_pkg.all;
entity t_vhdl_nine_pkgrec_sub is
  port (WishboneBus : inout wb_t);
end;
architecture a of t_vhdl_nine_pkgrec_sub is
begin
  InitRec(WishboneBus);
  process
  begin
    WishboneBus.Ack <= '0';
    wait until WishboneBus.Cyc = '1' and WishboneBus.Stb = '1';
    report "sub sees adr=" & to_hstring(WishboneBus.Adr) & " at " & time'image(now);
    WishboneBus.Ack <= '1' after 2 ns;
    wait;
  end process;
end;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_nine_pkgrec_pkg.all;
entity t_vhdl_nine_pkgrec is end;
architecture t of t_vhdl_nine_pkgrec is
  package Subordinate is new work.t_vhdl_nine_pkgrec_sigs generic map (W => 8);
begin
  d : entity work.t_vhdl_nine_pkgrec_dut port map (cyc_o => Subordinate.Cyc, stb_o => Subordinate.Stb,
                                 adr_o => Subordinate.Adr, ack_i => Subordinate.Ack);
  s : entity work.t_vhdl_nine_pkgrec_sub port map (WishboneBus => Subordinate.WishboneBus);
end;
