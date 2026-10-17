-- DESCRIPTION: Verilator: VHDL run-time check: a slice running the other way from its prefix, with ends only running gives, (NVC issue1231)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_slicedir_dyn_bad is
end entity;

architecture a of t_vhdl_slicedir_dyn_bad is
  function low_part(v : bit_vector; n : natural) return bit_vector is
  begin
    return v(n - 1 downto 0);
  end function;
begin
  process is
    variable d : bit_vector(7 downto 0) := "10100101";
    variable t : bit_vector(0 to 7) := "10100101";
  begin
    assert low_part(d, 4) = "0101" severity failure;
    report "downto prefix fits";
    assert low_part(t, 4) = "0101";
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
