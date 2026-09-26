-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- An array of integer_vector, handed aggregates of aggregates:
-- "sample_cross_bins(v_cross_x2, ((30, 1030), (35, 1035)), 1)" in UVVM's
-- functional coverage testbench (uvvm_util/tb/maintenance_tb/
-- func_cov_tb.vhd). Such an aggregate had no way to become a value held two
-- levels deep, and was refused; and a loop over "values(j)'range" ran over
-- the array's range rather than the element's.
entity t_vhdl_intarrarr is end entity;
architecture a of t_vhdl_intarrarr is
  type t_integer_array is array (natural range <>) of integer_vector;
  procedure sample(constant values : in t_integer_array; variable total : inout integer;
                   variable count : inout integer) is
  begin
    for j in values'range loop
      for k in values(j)'range loop
        total := total + values(j)(k);
      end loop;
      count := count + 1;
    end loop;
  end procedure;
begin
  process
    variable total, count : integer := 0;
  begin
    sample((0 => (20, 1020)), total, count);
    sample(((30, 1030), (35, 1035), (39, 1039)), total, count);
    report integer'image(total) & " " & integer'image(count);
    assert total = 20 + 1020 + 30 + 1030 + 35 + 1035 + 39 + 1039 and count = 4 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
