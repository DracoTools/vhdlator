-- DESCRIPTION: Verilator: VHDL generic of a logic type compared with a metavalue
--
-- This file is part of Verilator.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
--
-- A generic's value is settled by elaboration, so whether it is 'X' is
-- known exactly, although a two-state value cannot hold it. VUnit's
-- axi_stream_master reports an error unless "drive_invalid_val = 'X'", its
-- default. Checked here: a default, an actual, a generic handed on from an
-- enclosing one, and a component's own default.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_genmetaval_leaf is
  generic (
    v      : std_logic := 'X';
    want_x : boolean;
    want_u : boolean);
end entity;

architecture a of t_vhdl_genmetaval_leaf is
begin
  process
    impure function is_x return boolean is
    begin
      return v = 'X';
    end;
  begin
    assert (v = 'X') = want_x report "v = 'X' in " & t_vhdl_genmetaval_leaf'path_name severity failure;
    assert ('X' = v) = want_x report "'X' = v" severity failure;
    assert (v /= 'X') = not want_x report "v /= 'X'" severity failure;
    assert is_x = want_x report "in a function" severity failure;
    assert (v = 'U') = want_u report "v = 'U'" severity failure;
    assert (v = '-') = false report "v = '-'" severity failure;
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_genmetaval_mid is
  generic (w : std_logic := '0');
end entity;

architecture a of t_vhdl_genmetaval_mid is
begin
  -- Handed on from this entity's own generic
  l : entity work.t_vhdl_genmetaval_leaf
    generic map (v => w, want_x => false, want_u => true);
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_genmetaval is
end entity;

architecture a of t_vhdl_genmetaval is
  component t_vhdl_genmetaval_leaf is
    generic (
      v      : std_logic := 'U';
      want_x : boolean;
      want_u : boolean);
  end component;
begin
  -- Left at the entity's default
  dflt : entity work.t_vhdl_genmetaval_leaf
    generic map (want_x => true, want_u => false);
  -- Given a value that is not a metavalue
  one : entity work.t_vhdl_genmetaval_leaf
    generic map (v => '1', want_x => false, want_u => false);
  zero : entity work.t_vhdl_genmetaval_leaf
    generic map (v => '0', want_x => false, want_u => false);
  -- Given 'X' explicitly
  ex : entity work.t_vhdl_genmetaval_leaf
    generic map (v => 'X', want_x => true, want_u => false);
  -- 'U' through an enclosing entity's generic
  mid : entity work.t_vhdl_genmetaval_mid
    generic map (w => 'U');
  -- The component's default, 'U', and not the entity's
  comp : t_vhdl_genmetaval_leaf
    generic map (want_x => false, want_u => true);

  process
  begin
    wait for 1 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
