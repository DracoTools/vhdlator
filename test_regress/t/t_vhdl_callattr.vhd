-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- Array attributes of what a call gives back: "to_unsigned(n, G)'length",
-- "ints(n)'low" of a result that carries its own bounds. 'HIGH and 'LOW
-- read the result twice, which end being which depends on its direction,
-- and a call was refused as something that cannot be read twice; it is now
-- kept once. nvc 1.22.1 and Questa 25.1std run this; GHDL 7.0.0-dev
-- refuses a call as the prefix of an array attribute ("must be an object
-- name").
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_callattr_sub is
  generic (G : natural);
end entity;

architecture a of t_vhdl_callattr_sub is
  type t_slv_array is array (natural range <>) of std_logic_vector;
begin
  process
    variable calls : natural := 0;
    function words(n, w : natural) return t_slv_array is
      variable r : t_slv_array(1 to n)(w - 1 downto 0);
    begin
      return r;
    end function;
    impure function ints(n : natural) return integer_vector is
      variable r : integer_vector(n downto 1) := (others => 0);
    begin
      calls := calls + 1;
      return r;
    end function;
    variable n : natural := 3;
    variable w : natural := 12;
  begin
    assert to_unsigned(n, G)'length = G severity failure;
    assert words(n, w)'length = 3 and words(n, w)(3)'length = 12 severity failure;
    assert ints(n)'left = 3 and not ints(n)'ascending severity failure;
    calls := 0;
    assert ints(n)'low = 1 and ints(n)'high = 3 severity failure;
    -- Each call made once
    assert calls = 2 report integer'image(calls) severity failure;
    wait;
  end process;
end architecture;

entity t_vhdl_callattr is
end entity;

architecture a of t_vhdl_callattr is
begin
  u12 : entity work.t_vhdl_callattr_sub generic map (G => 12);
  u5 : entity work.t_vhdl_callattr_sub generic map (G => 5);

  process
  begin
    wait for 1 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
