-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Which of several declarations of one name a use denotes: an inner one
-- hides an outer one of the same profile (12.3); only what a protected type
-- declares is reached through an object of it (5.6.2); and an access value
-- is dereferenced without saying so where a name goes on to index it (8.1).

use std.env.all;

package outer_pkg is
  function which return string;
end package;

package body outer_pkg is
  function which return string is
  begin
    return "outer";
  end function;
end package body;

use std.env.all;
use work.outer_pkg.all;

entity t_vhdl_visible is
end entity;

architecture tb of t_vhdl_visible is

  -- A homograph declared here hides the one the use clause brought in
  function which return string is
  begin
    return "inner";
  end function;

  type counter is protected
    procedure bump;
    impure function value return integer;
  end protected;

  type counter is protected body
    variable n : integer := 0;
    -- Local to the body, and not a method of the type
    procedure bump(by : integer) is
    begin
      n := n + by;
    end procedure;
    procedure bump is
    begin
      bump(1);
    end procedure;
    impure function value return integer is
    begin
      return n;
    end function;
  end protected body;

  type int_vec is array (1 to 3) of integer;
  type row is record
    v : int_vec;
  end record;
  type row_vec is array (1 to 2) of row;
  type row_vec_ptr is access row_vec;

  shared variable c : counter;

begin

  main : process is
    variable p : row_vec_ptr;
  begin
    assert which = "inner" report "inner declaration hides the outer" severity failure;
    assert work.outer_pkg.which = "outer" report "outer by selected name" severity failure;

    c.bump;
    c.bump;
    assert c.value = 2 report "protected method" severity failure;

    -- p is an access to an array; naming an element of it dereferences
    p := new row_vec'((v => (1, 2, 3)), (v => (4, 5, 6)));
    assert p(2).v(3) = 6 report "index through an access" severity failure;
    p(1).v(2) := 9;
    assert p(1).v(2) = 9 report "assign through an access" severity failure;
    deallocate(p);

    report "*-* All Finished *-*";
    stop;
    wait;
  end process;

end architecture;
