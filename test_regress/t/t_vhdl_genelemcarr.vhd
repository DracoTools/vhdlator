-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A function giving back an array of words whose count only running
-- decides and whose width a generic gives, in two instances with two
-- widths. The value carries its own bounds, in a structure whose elements
-- are as wide as the generic makes them; that structure was one the whole
-- design shared, which read the generic of an entity no instance
-- elaborates, and could not be both widths at once.
library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_genelemcarr_pkg is
  type t_slv_array is array (natural range <>) of std_logic_vector;
end package;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_genelemcarr_pkg.all;

entity t_vhdl_genelemcarr_sub is
  generic (G : natural);
end entity;

architecture a of t_vhdl_genelemcarr_sub is
begin
  process
    impure function mk(n : natural) return t_slv_array is
      variable r : t_slv_array(0 to n - 1)(G - 1 downto 0);
    begin
      for i in r'range loop
        r(i) := (others => '1');
        r(i)(i) := '0';
      end loop;
      return r;
    end function;
    variable len : natural := 3;
    variable v : t_slv_array(0 to 2)(G - 1 downto 0);
  begin
    v := mk(len);
    assert v(0)(0) = '0' and v(0)(1) = '1' and v(2)(2) = '0' and v(2)(G - 1) = '1'
      severity failure;
    assert v(1)'length = G severity failure;
    wait;
  end process;
end architecture;

entity t_vhdl_genelemcarr is
end entity;

architecture a of t_vhdl_genelemcarr is
begin
  u8 : entity work.t_vhdl_genelemcarr_sub generic map (G => 8);
  u12 : entity work.t_vhdl_genelemcarr_sub generic map (G => 12);

  process
  begin
    wait for 1 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
