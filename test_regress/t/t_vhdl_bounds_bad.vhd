-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Every diagnostic bounds checking must give. Each line marked "Error" is one
-- the analyzer has to report at analysis time.

entity t_vhdl_bounds_bad is
end entity;

architecture rtl of t_vhdl_bounds_bad is
  type vec_t is array (positive range <>) of integer;
  type small_t is range 1 to 5;
  type state_t is (idle, busy, done);
  subtype narrow_t is integer range 0 to 7;

  signal s : vec_t(1 to 10);
  signal n : vec_t(1 downto 10);

  constant bad_const : narrow_t := 8;              -- Error: outside the subtype
  constant bad_range : vec_t(0 to 3) := (0, 1, 2, 3);  -- Error: 0 is not positive

  function widen (x : narrow_t) return integer;

begin

  s(0) <= 1;                                       -- Error: index below the range
  s(11) <= 1;                                      -- Error: index above the range
  s(1 to 11) <= (others => 0);                     -- Error: slice outside the array
  s(3 downto 1) <= (others => 0);                  -- Error: slice runs the other way
  s <= (1, 2, 3);                                  -- Error: three elements, ten wanted
  n <= (1 => 5, 3 => 6);                           -- Error: choices are not contiguous
  s <= (1 => 5, 1 => 6, others => 0);              -- Error: index given twice

  process is
    variable v : narrow_t;
    variable st : state_t;
  begin
    v := 9;                                        -- Error: outside the subtype
    widen_call : assert widen(-1) = 0;             -- Error: actual outside the subtype
    case st is
      when idle => null;
      when busy => null;
    end case;                                      -- Error: no alternative for done
    case v is
      when 0 to 4 => null;
      when 3 to 7 => null;                         -- Error: 3 is chosen twice
    end case;
    wait for -1 ns;                                -- Error: negative timeout
  end process;

  process is
  begin
    s(1) <= 1, 2;                                  -- Error: second element is not later
    wait;
  end process;

  process is
  begin
    assert s'length(3) = 0;                        -- Error: no third dimension
    wait;
  end process;

end architecture;
