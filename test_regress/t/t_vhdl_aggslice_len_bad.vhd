-- DESCRIPTION: Verilator: VHDL run-time check: a value of the aggregate's own type, of another length than its choice, (NVC bounds42)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_aggslice_len_bad is
end entity;

architecture a of t_vhdl_aggslice_len_bad is
  constant c1 : bit_vector(1 to 3) := "111";
  constant c2 : bit_vector(1 to 8) := x"00";
begin
  process is
    procedure test(x : bit_vector) is
    begin
      report to_string(bit_vector'(x'range => (3 downto 0 => '0', 7 downto 4 => '1')));
    end procedure;
  begin
    test(c2);
    test(c1);
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
