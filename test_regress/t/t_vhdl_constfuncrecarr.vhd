-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A function filling an array of records element by element,
-- "retConf(i).baseAddr := addr", gives a constant whose element sizes a
-- vector, so it is worked out as the design is elaborated. Shaped after
-- surf's genAxiLiteConfig (AxiLitePkg).

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package t_vhdl_constfuncrecarr_pkg is
  type CfgType is record
    baseAddr : std_logic_vector(31 downto 0);
    addrBits : natural;
    connectivity : std_logic_vector(15 downto 0);
  end record;
  type CfgArray is array (natural range <>) of CfgType;
  function gen (num : positive; base : std_logic_vector(31 downto 0); addrBits : natural) return CfgArray;
end package;

package body t_vhdl_constfuncrecarr_pkg is
  function gen (num : positive; base : std_logic_vector(31 downto 0); addrBits : natural) return CfgArray is
    variable retConf : CfgArray(num - 1 downto 0);
    variable addr : std_logic_vector(31 downto 0);
  begin
    addr := base;
    for i in 0 to num - 1 loop
      addr(23 downto addrBits) := std_logic_vector(to_unsigned(i, 24 - addrBits));
      retConf(i).baseAddr := addr;
      retConf(i).addrBits := addrBits;
      retConf(i).connectivity := x"FFFF";
    end loop;
    return retConf;
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_constfuncrecarr_pkg.all;

entity t_vhdl_constfuncrecarr is
end entity;

architecture rtl of t_vhdl_constfuncrecarr is
  constant N_C : positive := 2;
  constant C : CfgArray(N_C - 1 downto 0) := gen(N_C, x"00000000", 20);
  signal w : std_logic_vector(C(1).addrBits - 1 downto 0);
begin
  process
  begin
    report to_hstring(C(1).baseAddr) & " " & integer'image(w'length);
    assert C(1).baseAddr = x"00100000" and w'length = 20 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
