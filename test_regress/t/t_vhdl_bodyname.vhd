-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package timeunits is
  function pick(a : integer) return integer;
  procedure take(constant n : in integer; constant tag : in string);
end package;

package body timeunits is

  -- TIME declares timeunits called MIN and HR in STD.STANDARD. A subprogram of
  -- a package body may be called either, and it is not a completion of them.
  function min(a, b : integer) return integer is
  begin
    if a <= b then return a; end if;
    return b;
  end function;

  function hr(a, b : integer) return integer is
  begin
    if a >= b then return a; end if;
    return b;
  end function;

  function pick(a : integer) return integer is
  begin
    return min(a, 3) + hr(a, 3);
  end function;

  procedure take(constant n : in integer; constant tag : in string) is
  begin
    assert n = 8 report "n is " & integer'image(n) severity failure;
    report tag & " " & integer'image(n);
  end procedure;

end package body;

-- An overloaded name where the result alone tells the readings apart, and a
-- procedure of the same name beside them
package fetch is
  function one(constant k : integer) return integer;
  function one(constant k : integer) return boolean;
  function one(constant k : integer) return string;
  procedure one(constant k : in integer);
end package;

package body fetch is
  function one(constant k : integer) return integer is
  begin
    return k + 3;
  end function;
  function one(constant k : integer) return boolean is
  begin
    return k > 0;
  end function;
  function one(constant k : integer) return string is
  begin
    return "s" & integer'image(k);
  end function;
  procedure one(constant k : in integer) is
  begin
    report "proc " & integer'image(k);
  end procedure;
end package body;

use work.timeunits.all;
use work.fetch.all;

entity t_vhdl_bodyname is
end entity;

architecture a of t_vhdl_bodyname is
begin
  main : process
  begin
    assert pick(5) = 8 report "pick is " & integer'image(pick(5)) severity failure;

    -- The parameter's type is what picks which "one" is meant, and the
    -- procedure of that name is no obstacle
    take(one(5), "took");

    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
