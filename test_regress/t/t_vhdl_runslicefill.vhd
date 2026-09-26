-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- "(others => '0')" written into a slice whose ends only running settles:
-- UVVM's data queue flushing "priv_data_buffer(priv_max_idx(queue_idx)
-- downto priv_min_idx(queue_idx))" (uvvm_util/src/data_queue_pkg.vhd). The
-- aggregate has no length of its own, it is as long as the slice
-- (9.3.3.3), and was lowered as a part select of ends that are not
-- constants.
library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_runslicefill_pkg is
  type t_queue is protected
    procedure fill(hi, lo : natural);
    impure function get return std_logic_vector;
  end protected;
end package;

package body t_vhdl_runslicefill_pkg is
  type t_queue is protected body
    variable buf : std_logic_vector(31 downto 0) := (others => '1');
    procedure fill(hi, lo : natural) is
    begin
      buf(hi downto lo) := (others => '0');
    end procedure;
    impure function get return std_logic_vector is
    begin
      return buf;
    end function;
  end protected body;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_runslicefill_pkg.all;

entity t_vhdl_runslicefill is
end entity;

architecture a of t_vhdl_runslicefill is
  shared variable q : t_queue;
begin
  process
    variable buf : std_logic_vector(31 downto 0) := (others => '1');
    variable hi : natural := 11;
    variable lo : natural := 4;
    variable asc : std_logic_vector(0 to 15) := (others => '1');
  begin
    buf(hi downto lo) := (others => '0');
    assert buf = x"fffff00f" report to_hstring(buf) severity failure;
    hi := 30;
    lo := 28;
    buf(hi downto lo) := (others => '0');
    assert buf = x"8ffff00f" report to_hstring(buf) severity failure;
    asc(lo - 20 to hi - 20) := (others => '0');
    assert asc = x"ff1f" report to_hstring(asc) severity failure;
    -- A null slice writes nothing
    lo := 12;
    hi := 11;
    buf(hi downto lo) := (others => '1');
    assert buf = x"8ffff00f" report to_hstring(buf) severity failure;
    q.fill(15, 8);
    assert q.get = x"ffff00ff" report to_hstring(q.get) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
