-- DESCRIPTION: Verilator: VHDL 'others' aggregate assigned through an access value
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

-- The subtype of "p.all" is the designated type, an unconstrained array, so an
-- aggregate with 'others' has no index range to take (9.3.3.3)

entity t_vhdl_othersall_bad is
end entity;

architecture a of t_vhdl_othersall_bad is
  type int_ptr is access integer_vector;
begin
  process is
    variable p : int_ptr;
  begin
    p := new integer_vector(0 to 3);
    p.all := (others => 0);
    wait;
  end process;
end architecture;
