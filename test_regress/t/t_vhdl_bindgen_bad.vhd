-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- The formals of a binding indication's generic map are the generics of the
-- entity it names (7.3.2.2), so a name that is none of them is an error --
-- and one the analyzer has to find, since whether the binding is ever
-- elaborated depends on which unit the design is built from.

entity t_vhdl_bindgen_bad_sub is
  generic (width : positive);
  port (o : out integer);
end entity;

architecture rtl of t_vhdl_bindgen_bad_sub is
begin
  o <= width;
end architecture;

entity t_vhdl_bindgen_bad is
end entity;

architecture tb of t_vhdl_bindgen_bad is
  component t_vhdl_bindgen_bad_sub is
    generic (width : positive := 4);
    port (o : out integer);
  end component;
  signal s : integer := 0;
begin
  u1: t_vhdl_bindgen_bad_sub port map (o => s);
end architecture;

configuration t_vhdl_bindgen_bad_cfg of t_vhdl_bindgen_bad is
  for tb
    for u1 : t_vhdl_bindgen_bad_sub
      use entity work.t_vhdl_bindgen_bad_sub
        generic map (widht => 3);
    end for;
  end for;
end;
