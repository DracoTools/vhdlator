-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A signal parameter may be given an alias of an external name (8.7, 4.2.2.3;
-- GHDL gna issue2969). The procedure forces that other unit's signal, and an
-- unconstrained formal takes the subtype of the actual.

package t_vhdl_ename_sigactual_pkg is
  type rec_t is record
    a : bit_vector(3 downto 0);
    b : bit;
  end record;
end package;

use work.t_vhdl_ename_sigactual_pkg.all;

entity t_vhdl_ename_sigactual_sub is
end entity;

architecture behav of t_vhdl_ename_sigactual_sub is
  signal sig : bit_vector(21 downto 0) := (others => '0');
  signal r : rec_t := ("0000", '0');
begin
end architecture;

use work.t_vhdl_ename_sigactual_pkg.all;

entity t_vhdl_ename_sigactual is
end entity;

architecture behav of t_vhdl_ename_sigactual is
begin
  sub : entity work.t_vhdl_ename_sigactual_sub;

  process
    alias whole is <<signal sub.sig : bit_vector(21 downto 0)>>;
    procedure bounds (signal arg : in bit_vector; l, r : integer) is
    begin
      assert arg'left = l and arg'right = r
        report integer'image(arg'left) & " downto " & integer'image(arg'right)
        severity failure;
    end procedure;
    procedure set_bit (signal arg : inout bit_vector) is
      constant at : natural := arg'length - 4;
    begin
      arg(at) <= force '1';
    end procedure;
  begin
    bounds(whole, 21, 0);
    set_bit(whole);
    wait for 1 ns;
    assert whole(18) = '1' and whole(17) = '0' report "not forced" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
