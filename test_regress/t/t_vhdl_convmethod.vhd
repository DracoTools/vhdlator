-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A method of a protected type returning an array of two elements,
-- converted to INTEGER_VECTOR and handed to a procedure whose parameter
-- leaves the length open: UVVM's func_cov_pkg, "write_value(integer_vector(
-- priv_rand_gen.get_rand_seeds(VOID)))". A conversion keeps its operand's
-- bounds (9.3.6), but the operand here is a method call, whose length was
-- not looked for, so the array went where a value carrying its length was
-- wanted and the C++ did not compile. Every UVVM testbench that uses the
-- functional coverage package stopped there.
package t_vhdl_convmethod_pkg is
  type t_pos_vec is array (natural range <>) of positive;
  type gen_t is protected
    impure function seeds(k : natural) return t_pos_vec;
  end protected;
  type user_t is protected
    procedure dump;
  end protected;
end package;
package body t_vhdl_convmethod_pkg is
  type gen_t is protected body
    variable s1 : positive := 4;
    impure function seeds(k : natural) return t_pos_vec is
      variable r : t_pos_vec(0 to 1);
    begin
      r(0) := s1 + k;
      r(1) := s1 + k + 1;
      return r;
    end function;
  end protected body;
  type user_t is protected body
    variable gen : gen_t;
    variable count : natural := 3;
    procedure dump is
      variable total : integer := 0;
      procedure write_value(constant value : in integer_vector) is
      begin
        for i in 0 to value'length - 1 loop
          total := total + value(i);
        end loop;
      end procedure;
      procedure write_value(constant value : in integer) is
      begin
        total := total + value;
      end procedure;
    begin
      write_value(count);
      write_value(integer_vector(gen.seeds(1)));
      assert total = 14 report "total " & integer'image(total) severity failure;
      report "*-* All Finished *-*";
    end procedure;
  end protected body;
end package body;

use work.t_vhdl_convmethod_pkg.all;
entity t_vhdl_convmethod is
end entity;
architecture a of t_vhdl_convmethod is
  shared variable u : user_t;
begin
  process
  begin
    u.dump;
    wait;
  end process;
end architecture;
