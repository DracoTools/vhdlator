-- DESCRIPTION: Verilator: VHDL bound checks in the name of an access value
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

-- UVVM's VVC register: an array of records, each holding an access value,
-- indexed by a variable, then written and read through the access value

entity t_vhdl_boundcheck_access is
end entity;

architecture a of t_vhdl_boundcheck_access is
  type int_arr is array (natural range <>) of integer;
  type int_arr_ptr is access int_arr;
  type state_t is record
    list : int_arr_ptr;
    count : natural;
  end record;
  type state_arr is array (0 to 3) of state_t;

  type registry_t is protected
    procedure add (n : positive);
    impure function total (i : natural) return integer;
    impure function length_of (i : natural) return natural;
  end protected;

  type registry_t is protected body
    variable regs : state_arr;
    variable last : integer := -1;
    procedure add (n : positive) is
    begin
      last := last + 1;
      regs(last).list := new int_arr(0 to n - 1);
      regs(last).list.all := (0 to n - 1 => last + 1);
      regs(last).list(0) := 10 * (last + 1);
      regs(last).count := regs(last).list'length;
    end procedure;
    impure function total (i : natural) return integer is
      variable sum : integer := 0;
    begin
      for k in regs(i).list'range loop
        sum := sum + regs(i).list(k);
      end loop;
      return sum;
    end function;
    impure function length_of (i : natural) return natural is
    begin
      return regs(i).list'length;
    end function;
  end protected body;

  shared variable reg : registry_t;
begin
  process is
  begin
    reg.add(3);
    reg.add(2);
    -- 10 + 1 + 1, and 20 + 2
    assert reg.total(0) = 12 severity failure;
    assert reg.total(1) = 22 severity failure;
    assert reg.length_of(0) = 3 and reg.length_of(1) = 2 severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
