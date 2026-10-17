-- DESCRIPTION: Verilator: VHDL run-time check: a range constraint outside the subtype it narrows, settled by elaboration, (NVC issue1394)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_subrange_elab_bad is
end entity;

architecture a of t_vhdl_subrange_elab_bad is
  function to_max(a, b : integer) return integer is
  begin
    if a >= b then
      return a;
    end if;
    return b;
  end function;
  constant count_c : positive := to_max(7, 8);
begin
  process is
    variable section_v : positive range 0 to count_c - 1;
  begin
    section_v := 2;
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
