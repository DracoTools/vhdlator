-- DESCRIPTION: Verilator: VHDL run-time check: a port map actual of another length than its port (NVC bounds44)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_portact_len_bad is
end entity;

architecture a of t_vhdl_portact_len_bad is
  function get_bits(b : bit; n : natural) return bit_vector is
  begin
    return (1 to n => b);
  end function;
begin
  b : block is
    port (p : in bit_vector(1 to 2));
    port map (p => inertial get_bits('1', 5));
  begin
  end block;

  process is
  begin
    wait for 1 ns;
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
