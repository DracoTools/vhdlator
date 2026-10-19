-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A user-defined attribute of an unconstrained array type: the value an
-- attribute specification gives it is as long as it is (7.2; GHDL gna issue543)

entity t_vhdl_attrvec is
end entity;

architecture a of t_vhdl_attrvec is
  attribute att : integer_vector;
  constant const : integer := 1;
  attribute att of const : constant is (2, 3);
  constant const2 : integer_vector := const'att;
  constant const3 : integer_vector := const2;
begin
  process is
  begin
    assert const3'length = 2 and const3(0) = 2 and const3(1) = 3 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
