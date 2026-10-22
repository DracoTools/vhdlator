-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An architecture's constant of a record of a boolean, an integer and an
-- enumeration, handed to a package function that another constant calls:
-- the function is worked out as the design is elaborated, so the record it
-- is given has to be a value elaboration settles. Shaped after surf's
-- AxiStreamDepacketizer, "axiStreamMasterInit(AXIS_CONFIG_C)" in REG_INIT_C.

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_recconstarg_pkg is
  type ModeType is (M_A, M_B);
  type CfgType is record
    EN_C : boolean;
    BYTES_C : natural;
    MODE_C : ModeType;
  end record;
  type MType is record
    tValid : std_logic;
    tKeep : std_logic_vector(7 downto 0);
  end record;
  function mInit (cfg : CfgType) return MType;
end package;

package body t_vhdl_recconstarg_pkg is
  function mInit (cfg : CfgType) return MType is
    variable ret : MType := (tValid => '0', tKeep => (others => '0'));
  begin
    if cfg.MODE_C = M_B and cfg.EN_C then
      ret.tKeep := x"07";
      ret.tValid := '1';
    end if;
    return ret;
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_recconstarg_pkg.all;

entity t_vhdl_recconstarg is
end entity;

architecture rtl of t_vhdl_recconstarg is
  constant CFG_C : CfgType := (EN_C => true, BYTES_C => 3, MODE_C => M_B);
  type MArray is array (natural range <>) of MType;
  type RegType is record
    outs : MArray(1 downto 0);
  end record;
  constant REG_INIT_C : RegType := (outs => (others => mInit(CFG_C)));
  signal r : RegType := REG_INIT_C;
begin
  process
  begin
    wait for 1 ns;
    report to_hstring(r.outs(1).tKeep);
    assert r.outs(1).tKeep = x"07" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
