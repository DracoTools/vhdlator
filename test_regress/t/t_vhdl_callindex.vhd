-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- An element of what a call gives back, where the result carries its own
-- bounds: "mk(len)(2)". Every part of such a value -- its elements, its left
-- bound, its direction -- is a separate read of it, so it was refused as
-- anything but a name. It is now worked out once and indexed as the object
-- that keeps it.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package t_vhdl_callindex_pkg is
  type t_slv_array is array (natural range <>) of std_logic_vector;
end package;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.t_vhdl_callindex_pkg.all;

entity t_vhdl_callindex_sub is
  generic (G : natural);
end entity;

architecture a of t_vhdl_callindex_sub is
begin
  process
    variable calls : natural := 0;
    -- Words a generic sizes
    impure function mk(n : natural) return t_slv_array is
      variable r : t_slv_array(0 to n - 1)(G - 1 downto 0);
    begin
      calls := calls + 1;
      for i in r'range loop
        r(i) := std_logic_vector(to_unsigned(i + 1, G));
      end loop;
      return r;
    end function;
    -- Words the call sizes: held two levels deep
    function words(n, w : natural) return t_slv_array is
      variable r : t_slv_array(1 to n)(w - 1 downto 0);
    begin
      for i in r'range loop
        r(i) := std_logic_vector(to_unsigned(16 * i, w));
      end loop;
      return r;
    end function;
    -- Integers, counted downwards
    function ints(n : natural) return integer_vector is
      variable r : integer_vector(n downto 1);
    begin
      for i in r'range loop
        r(i) := i * 10;
      end loop;
      return r;
    end function;
    variable len : natural := 3;
    variable w : natural := 12;
  begin
    assert mk(len)(2) = std_logic_vector(to_unsigned(3, G)) severity failure;
    -- The call is made once for the one element read
    assert calls = 1 severity failure;
    assert words(len, w)(2) = x"020" severity failure;
    assert ints(len)(3) = 30 and ints(len)(1) = 10 severity failure;
    wait;
  end process;
end architecture;

entity t_vhdl_callindex is
end entity;

architecture a of t_vhdl_callindex is
begin
  u8 : entity work.t_vhdl_callindex_sub generic map (G => 8);
  u12 : entity work.t_vhdl_callindex_sub generic map (G => 12);

  process
  begin
    wait for 1 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
