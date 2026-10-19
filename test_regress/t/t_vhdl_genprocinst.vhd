-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A generic procedure instantiated twice in the package that declares it,
-- before the package's body is analyzed (4.4; GHDL gna issue3090)

package t_vhdl_genprocinst_pkg is
  procedure ternary_generic generic (type T) parameter (cond : boolean; when_true : T;
                                                         when_false : T; res : out T);
  procedure ternary is new ternary_generic generic map (integer);
  procedure ternary is new ternary_generic generic map (boolean);
end package;

package body t_vhdl_genprocinst_pkg is
  procedure ternary_generic generic (type T) parameter (cond : boolean; when_true : T;
                                                         when_false : T; res : out T) is
  begin
    if cond then
      res := when_true;
    else
      res := when_false;
    end if;
  end procedure;
end package body;

use work.t_vhdl_genprocinst_pkg.ternary;

entity t_vhdl_genprocinst is
end entity;

architecture a of t_vhdl_genprocinst is
begin
  process is
    variable vint : integer;
    variable vbool : boolean;
  begin
    ternary(false, 1, 2, vint);
    ternary(true, true, false, vbool);
    assert vint = 2 and vbool severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
