-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A procedure with a signal parameter is copied for each signal a call
-- gives it, and two calls on elements of one record signal nested at
-- different depths -- "ifc.wr.resp" and "ifc.rd.resp", both two bits -- are
-- two signals. UVVM's AXI VVC checks each response signal for unwanted
-- activity this way; sharing one copy checked bresp where rresp changed.

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_sigcopyelem_pkg is
  type t_wr is record
    resp  : std_logic_vector(1 downto 0);
    valid : std_logic;
  end record;
  type t_rd is record
    resp  : std_logic_vector(1 downto 0);
    valid : std_logic;
  end record;
  type t_ifc is record
    wr : t_wr;
    rd : t_rd;
  end record;
end package;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_sigcopyelem_pkg.all;

entity t_vhdl_sigcopyelem is
end entity;

architecture a of t_vhdl_sigcopyelem is
  signal ifc : t_ifc := (wr => ("00", '0'), rd => ("00", '0'));
  signal seen : string(1 to 4) := (others => '.');

  procedure check(signal s : in std_logic_vector; variable hit : inout character) is
  begin
    if s'event then
      hit := 'x';
    end if;
  end procedure;
  procedure check(signal s : in std_logic; variable hit : inout character) is
  begin
    if s'event then
      hit := 'x';
    end if;
  end procedure;
begin
  process
    variable v1, v2, v3, v4 : character := '.';
  begin
    wait on ifc;
    check(ifc.wr.resp, v1);
    check(ifc.wr.valid, v2);
    check(ifc.rd.resp, v3);
    check(ifc.rd.valid, v4);
    seen <= (v1, v2, v3, v4);
  end process;

  process
  begin
    wait for 1 ns;
    ifc.rd.resp <= "11";
    wait for 1 ns;
    assert seen = "..x." report "after rd.resp: " & seen severity failure;
    ifc.wr.valid <= '1';
    wait for 1 ns;
    assert seen = ".xx." report "after wr.valid: " & seen severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
