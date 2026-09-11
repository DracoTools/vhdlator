-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A loop or generate runs over a discrete range. A function returns a value
-- of its result type. A case choice is locally static. A variable parameter
-- is given a variable, and a signal parameter or an output a static signal
-- name. A type's range is two bounds or a range attribute, and an array's
-- index a range; each row of an aggregate is as long as its dimension. A
-- resolution function takes an array of the type's values, or of its
-- element's, and returns one.
-- References: nvc 1.22.1, GHDL 7.0.0-dev, Questa 2025.2.

entity t_vhdl_stmtrules_sub is
  port (i : in bit; o : out bit);
end entity;

architecture a of t_vhdl_stmtrules_sub is
begin
end architecture;

entity t_vhdl_stmtrules_bad is
end entity;

architecture a of t_vhdl_stmtrules_bad is
  type mvl is ('0', '1', 'Z');
  type mvl_vec is array (natural range <>) of mvl;
  type mvl_vec2 is array (natural range <>) of mvl_vec(0 to 1);
  type grid is array (0 to 1, 0 to 2) of bit;
  type rec is record
    v : bit_vector(0 to 3);
  end record;
  function res (v : mvl_vec) return mvl is begin return v(v'left); end function;
  function res2 (v : bit_vector) return bit is begin return v(v'left); end function;
  procedure pv (variable x : in integer) is begin end procedure;
  procedure ps (signal x : in bit) is begin end procedure;
  signal sv : bit_vector(0 to 3);
  signal s : integer;
  -- Legal
  subtype rmvl is res mvl;
  subtype rmvl2d is ((res)) mvl_vec2;
  constant g0 : grid := (('1', '0', '1'), "010");
  function v (x : integer) return bit_vector is begin return "0000"; end function;
  function f return integer is begin return 1; end function;
  -- Illegal
  subtype rmvl2 is res2 mvl;
  type t2 is range integer;
  type t3 is array (5) of integer;
  constant g1 : grid := (('1', '0', '1'), ('1', '0'));
  function g return integer is begin return 1.5; end function;
begin
  -- Legal
  u1 : entity work.t_vhdl_stmtrules_sub port map (i => sv(1), o => sv(3));
  -- Illegal
  gen : for k in 0.0 to 1.0 generate end generate;
  -- Legal
  process
    variable v, n : integer := 0;
    variable r : rec;
  begin
    -- Legal
    pv(n);
    r.v(0 to 1) := "10";
    ps(sv(2));
    -- Illegal
    for j in 1.0 to 2.0 loop end loop;
    case n is when v => null; when others => null; end case;
    pv(s);
    ps(sv(n));
    wait;
  end process;
  -- Illegal
  u2 : entity work.t_vhdl_stmtrules_sub port map (i => sv(s), o => sv(s));
end architecture;
