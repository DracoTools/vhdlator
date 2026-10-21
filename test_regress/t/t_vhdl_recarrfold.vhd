-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A constant of a record of two arrays, worked out by a function that
-- writes one array and then reads it back to fill the other, sizes a
-- signal, so the instance works it out before the design runs (PoC's
-- arith_scaler: "constant DIV_PROPS : tDivProps := computeProps")

package t_vhdl_recarrfold_pkg is
  type T_POSVEC is array (natural range <>) of positive;
  function imin(vec : T_POSVEC) return positive;
end package;

package body t_vhdl_recarrfold_pkg is
  function imin(vec : T_POSVEC) return positive is
    variable r : positive := positive'high;
  begin
    for i in vec'range loop
      if vec(i) < r then
        r := vec(i);
      end if;
    end loop;
    return r;
  end function;
end package body;

use work.t_vhdl_recarrfold_pkg.all;
entity t_vhdl_recarrfold_sc is
  generic (DIVS : T_POSVEC := (0 => 1); N : positive := 8);
  port (width : out natural; align0 : out natural; align3 : out natural);
end entity;
architecture a of t_vhdl_recarrfold_sc is
  type tDivProps is record
    steps : T_POSVEC(DIVS'range);
    align : T_POSVEC(DIVS'range);
  end record;
  function computeProps return tDivProps is
    variable res : tDivProps;
    variable min_steps : positive;
  begin
    for i in DIVS'range loop
      res.steps(i) := N + 4 - i;
    end loop;
    min_steps := imin(res.steps);
    for i in DIVS'range loop
      res.align(i) := DIVS(i) * 2**(res.steps(i) - min_steps);
    end loop;
    return res;
  end function;
  constant DIV_PROPS : tDivProps := computeProps;
  constant MAXS : positive := DIV_PROPS.align(DIVS'left);
  signal wide : bit_vector(MAXS - 1 downto 0);
begin
  width <= wide'length;
  align0 <= DIV_PROPS.align(DIVS'left);
  align3 <= DIV_PROPS.align(DIVS'right);
end architecture;

use work.t_vhdl_recarrfold_pkg.all;
entity t_vhdl_recarrfold is
end entity;
architecture a of t_vhdl_recarrfold is
  constant D : T_POSVEC := (1, 2, 3, 4);
  signal width, align0, align3 : natural;
begin
  u : entity work.t_vhdl_recarrfold_sc generic map (DIVS => D)
    port map (width => width, align0 => align0, align3 => align3);
  process
  begin
    wait for 1 ns;
    assert width = 8 report "width " & integer'image(width) severity failure;
    assert align0 = 8 report "align0 " & integer'image(align0) severity failure;
    assert align3 = 4 report "align3 " & integer'image(align3) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
