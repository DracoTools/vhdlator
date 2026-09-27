-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- An array whose length only running settles starts with every element at
-- its subtype's default (6.4.2.2): an INTEGER at INTEGER'LEFT, not zero.
-- It was allocated zeroed. UVVM's functional coverage declares its loaded
-- bins "variable v_loaded_bins : t_cov_bin_vector(0 to loaded_bins_idx -
-- 1)", with a count read from the database file, and compares each whole
-- with the bins it made, whose unused values are INTEGER'LEFT: none
-- matched, and every bin was loaded twice (uvvm_util/src/func_cov_pkg.vhd,
-- load_coverage_db).
package t_vhdl_dyndefault_pkg is
  type t_kind is (VAL, RAN);
  type t_bin is record
    contains : t_kind;
    values : integer_vector(0 to 3);
    num_values : natural range 0 to 4;
  end record;
  type t_bin_vector is array (natural range <>) of t_bin;
  type t_cov_bin is record
    cross_bins : t_bin_vector(0 to 1);
    hits : natural;
    rand_weight : integer;
    level : real;
  end record;
  type t_cov_bin_vector is array (natural range <>) of t_cov_bin;
  subtype t_small is integer range 5 to 9;
  type t_small_vector is array (natural range <>) of t_small;
  type t_level is (LOW, MID, HIGH);
  subtype t_upper is t_level range MID to HIGH;
  type t_upper_vector is array (natural range <>) of t_upper;
end package;

use work.t_vhdl_dyndefault_pkg.all;

entity t_vhdl_dyndefault is
end entity;

architecture a of t_vhdl_dyndefault is
begin
  process
    variable n : natural := 3;

    procedure check(constant k : in natural) is
      variable iv : integer_vector(0 to k - 1);
      variable rv : real_vector(1 to k);
      variable sv : t_small_vector(k - 1 downto 0);
      variable uv : t_upper_vector(0 to k - 1);
      variable bv : t_bin_vector(0 to k - 1);
      variable cv : t_cov_bin_vector(0 to k - 1);
      variable made : t_bin;
    begin
      assert iv(1) = integer'left report "iv " & integer'image(iv(1)) severity failure;
      assert rv(2) = real'left severity failure;
      assert sv(0) = 5 report "sv " & integer'image(sv(0)) severity failure;
      assert uv(2) = MID severity failure;
      assert bv(2).values(3) = integer'left and bv(2).num_values = 0 severity failure;
      assert cv(1).cross_bins(1).values(0) = integer'left and cv(1).rand_weight = integer'left
        and cv(1).hits = 0 and cv(1).level = real'left severity failure;
      -- A bin made the same way, element by element, equals one of them
      made.values(0) := 10;
      made.num_values := 1;
      bv(0).values(0) := 10;
      bv(0).num_values := 1;
      assert bv(0) = made severity failure;
    end procedure;
  begin
    check(n);
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
