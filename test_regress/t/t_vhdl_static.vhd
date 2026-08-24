-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Static expression evaluation, type bounds and predefined attributes, all
-- of which must fold at analysis time (IEEE 1076-2008 clauses 5, 9.4, 16.2).

package p is
  -- Bounds of a type definition are locally static expressions
  type byte_t is range 0 to 2 ** 8 - 1;
  type level_t is range -1.5 to 1.5;
  type state_t is (idle, busy, done);
  type opcode_t is ('a', 'b', 'c');

  -- A physical type's secondary units fold against the ones before them
  type freq_t is range 0 to 1000000
    units
      hz;
      khz = 1000 hz;
      mhz = 1000 khz;
      ghz = 1000 mhz;
    end units;

  -- Constants fold, including through operators, attributes and concatenation
  constant width_c : integer := 4 * 2;
  constant mask_c : integer := 2 ** width_c - 1;
  constant neg_c : integer := -mask_c / 3;
  constant modulo_c : integer := (-7) mod 3;
  constant remain_c : integer := (-7) rem 3;
  constant high_c : integer := byte_t'high;
  constant pos_c : integer := state_t'pos(done);
  constant succ_c : state_t := state_t'succ(idle);
  constant image_c : string := "n=" & integer'image(42);
  constant value_c : integer := integer'value("17");
  constant real_c : real := 5.0 / 2;
  constant bool_c : boolean := 3 < 2;
  constant delay_c : time := 2 * 500 ns;
  constant char_c : character := 'x';

  -- Array bounds follow from the folded constants
  type word_t is array (natural range <>) of bit;
  subtype byte_vec_t is word_t(width_c - 1 downto 0);
  type matrix_t is array (0 to 1, 0 to 2) of integer;
  subtype small_t is integer range 1 to width_c;

  -- Attributes of a constrained array
  constant len_c : integer := byte_vec_t'length;
  constant left_c : integer := byte_vec_t'left;
  constant asc_c : boolean := byte_vec_t'ascending;
  constant dim2_c : integer := matrix_t'length(2);
end package;

entity t_vhdl_static is
end entity;

architecture rtl of t_vhdl_static is
  use work.p.all;
  -- One signal per assignment: a signal of an unresolved type may have only
  -- one driver (6.5.2), and each of these is a driver
  signal v : byte_vec_t;
  signal v2 : byte_vec_t;
  signal v3 : byte_vec_t;
  signal s : state_t;
begin
  -- Aggregates whose choices must cover the target exactly
  v <= (others => '0');
  v2 <= "10101010";
  v3 <= (7 downto 4 => '1', 3 downto 0 => '0');

  process is
    variable i : small_t;
  begin
    -- A case over a discrete subtype must name every value
    case s is
      when idle => i := 1;
      when busy => i := 2;
      when done => i := 3;
    end case;
    wait for 10 ns;
  end process;
end architecture;
