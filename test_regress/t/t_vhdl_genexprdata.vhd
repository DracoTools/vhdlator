-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A generic given an expression over a constant that calls work out from
-- 'PATH_NAME, "MODEL_ID_NAME => MODEL_INSTANCE_NAME & ".Rx"" in OSVVM's
-- Ethernet components: no value the back end folds, so the generic travels
-- as data (6.5.6.2, 16.2.5). Read before the process's first wait, as is a
-- constant the instance works out from it, in the variant that is not inlined
-- too, where the generic is an input of the instance's module.

package t_vhdl_genexprdata_pkg is
  function IfElse(Expr : boolean; A, B : string) return string;
  function PathTail(A : string) return string;
end package;
package body t_vhdl_genexprdata_pkg is
  function IfElse(Expr : boolean; A, B : string) return string is
  begin
    if Expr then return A; else return B; end if;
  end function;
  function PathTail(A : string) return string is
    alias aA : string(1 to A'length) is A;
    variable LenA : integer := A'length;
    variable Last : integer := LenA;
  begin
    if aA(LenA) = ':' then Last := LenA - 1; end if;
    for i in Last downto 1 loop
      if aA(i) = ':' then return aA(i + 1 to Last); end if;
    end loop;
    return aA(1 to Last);
  end function;
end package body;

use work.t_vhdl_genexprdata_pkg.all;
entity t_vhdl_genexprdata_leaf is
  generic (MODEL_ID_NAME : string := "");
  -- Worked out from the generic, which travels as data, so only once it has its value
  constant MODEL_INSTANCE_NAME : string :=
    IfElse(MODEL_ID_NAME'length > 0, MODEL_ID_NAME, PathTail(t_vhdl_genexprdata_leaf'PATH_NAME));
end;
architecture a of t_vhdl_genexprdata_leaf is
begin
  process begin
    report "leaf name=" & MODEL_ID_NAME & " instance=" & MODEL_INSTANCE_NAME;
    assert MODEL_INSTANCE_NAME = MODEL_ID_NAME severity failure;
    assert MODEL_ID_NAME = "phy_1.Rx" or MODEL_ID_NAME = "named.Rx" severity failure;
    wait;
  end process;
end;

use work.t_vhdl_genexprdata_pkg.all;
entity t_vhdl_genexprdata_phy is
  generic (MODEL_ID_NAME : string := "");
end;
architecture a of t_vhdl_genexprdata_phy is
  constant MODEL_INSTANCE_NAME : string :=
    IfElse(MODEL_ID_NAME'length > 0, MODEL_ID_NAME, PathTail(t_vhdl_genexprdata_phy'PATH_NAME));
begin
  rx : entity work.t_vhdl_genexprdata_leaf generic map (MODEL_ID_NAME => MODEL_INSTANCE_NAME & ".Rx");
end;

entity t_vhdl_genexprdata is
end entity;
architecture t of t_vhdl_genexprdata is
begin
  phy_1 : entity work.t_vhdl_genexprdata_phy;
  phy_2 : entity work.t_vhdl_genexprdata_phy generic map (MODEL_ID_NAME => "named");
  process
  begin
    wait for 1 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end;

