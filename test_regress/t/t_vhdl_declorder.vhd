-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A subprogram's declarations are elaborated in order on every call
-- (14.6), and a later one may read what an earlier one was given: VUnit's
-- string decode (vunit/vhdl/data_types/src/codec.vhd), "constant ret_range
-- : range_t := get_range(code); variable ret_val : string(ret_range'range)
-- := (others => NUL)". The variable's initial value was worked out on
-- entry, before the constant had its bounds, so the string was one
-- character long and every string VUnit popped from a queue came back cut.
entity t_vhdl_declorder is
end entity;

architecture a of t_vhdl_declorder is
  type range_t is array (integer range <>) of bit;

  -- The bounds, read out of the code itself
  function get_range(constant code : string) return range_t is
    constant l : integer := character'pos(code(code'left));
    constant r : integer := character'pos(code(code'left + 1));
    constant up : range_t(l to r) := (others => '0');
    constant dn : range_t(l downto r) := (others => '0');
  begin
    if code(code'left + 2) = 'T' then
      return up;
    else
      return dn;
    end if;
  end function;

  function decode(constant code : string) return string is
    constant rr : range_t := get_range(code);
    variable v : string(rr'range) := (others => NUL);
    constant n : natural := v'length;
    variable w : string(1 to n) := (others => '.');
  begin
    v := code(code'left + 3 to code'left + 2 + v'length);
    return v & w;
  end function;
begin
  process
  begin
    assert decode(character'val(1) & character'val(5) & "Talpha") = "alpha....."
      report decode(character'val(1) & character'val(5) & "Talpha") severity failure;
    assert decode(character'val(3) & character'val(2) & "Fbe") = "be.."
      report decode(character'val(3) & character'val(2) & "Fbe") severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
