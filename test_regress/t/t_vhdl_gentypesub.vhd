-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

-- A generic package with a type of its own and an operation on it
package boxes is
  generic (type t_element;
           function to_str(e : t_element) return string);

  procedure show(constant e : in t_element);

  type t_box is protected
    procedure put(constant e : in t_element);
    impure function count return integer;
  end protected;
end package;

package body boxes is
  procedure show(constant e : in t_element) is
  begin
    report "e is " & to_str(e);
  end procedure;

  type t_box is protected body
    variable priv_n : integer := 0;
    procedure put(constant e : in t_element) is
    begin
      priv_n := priv_n + 1;
      report "put " & to_str(e);
    end procedure;
    impure function count return integer is
    begin
      return priv_n;
    end function;
  end protected body;
end package body;

-- The actual is a subtype indication, constraint and all
library ieee;
use ieee.std_logic_1164.all;
package byte_boxes is new work.boxes
  generic map (t_element => std_logic_vector(7 downto 0), to_str => to_string);

library ieee;
use ieee.std_logic_1164.all;
package word_boxes is new work.boxes
  generic map (t_element => std_logic_vector(15 downto 0), to_str => to_string);

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_gentypesub is
end entity;

architecture a of t_vhdl_gentypesub is
  -- Two instances of one generic package: two protected types, each its own
  shared variable b8 : work.byte_boxes.t_box;
  shared variable b16 : work.word_boxes.t_box;
begin
  main : process
  begin
    work.byte_boxes.show(x"5a");
    work.word_boxes.show(x"1234");

    b8.put(x"5a");
    b8.put(x"a5");
    b16.put(x"1234");

    assert b8.count = 2 report "b8 count is " & integer'image(b8.count) severity failure;
    assert b16.count = 1 report "b16 count is " & integer'image(b16.count) severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
