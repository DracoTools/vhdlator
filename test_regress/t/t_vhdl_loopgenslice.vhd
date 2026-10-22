-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A slice whose end is an element of a generic array indexed by a loop's
-- parameter, "v(m).awaddr(31 downto CFG_G(m).addrBits) :=
-- CFG_G(m).baseAddr(31 downto CFG_G(m).addrBits)": the end names a generic
-- but takes a new value on each iteration (10.10), so the slice's length is
-- running's to decide. Shaped after surf's AxiLiteCrossbar.

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_loopgenslice_pkg is
  type CfgType is record
    baseAddr : std_logic_vector(31 downto 0);
    addrBits : natural;
  end record;
  type CfgArray is array (natural range <>) of CfgType;
  type MType is record
    awaddr : std_logic_vector(31 downto 0);
  end record;
  type MArray is array (natural range <>) of MType;
end package;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_loopgenslice_pkg.all;

entity t_vhdl_loopgenslice is
  generic (N_G : natural := 2;
           CFG_G : CfgArray := ((baseAddr => x"12340000", addrBits => 16),
                                (baseAddr => x"ABCDEF00", addrBits => 8)));
end entity;

architecture rtl of t_vhdl_loopgenslice is
  signal r : MArray(N_G - 1 downto 0) := (others => (awaddr => x"FFFFFFFF"));
begin
  process
    variable v : MArray(N_G - 1 downto 0);
  begin
    v := r;
    for m in N_G - 1 downto 0 loop
      if CFG_G(m).addrBits /= 32 then
        v(m).awaddr(31 downto CFG_G(m).addrBits) := CFG_G(m).baseAddr(31 downto CFG_G(m).addrBits);
      end if;
    end loop;
    report to_hstring(v(0).awaddr) & " " & to_hstring(v(1).awaddr);
    assert v(0).awaddr = x"1234FFFF" severity failure;
    assert v(1).awaddr = x"ABCDEFFF" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
