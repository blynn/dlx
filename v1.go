// Too slow! Investigate.

package main

import "bufio"
import "fmt"
import "os"
import "strings"

var sym [][]string

func find(s string) (int, int) {
  for i, b := range sym {
    for j, c := range b {
      if c == s {
        return i, j
      }
    }
  }
  return -1, -1
}

type rule struct {
  kind string
  arg []int
}

var rules []rule

func add(ss []string) {
  r := new(rule)
  first := true
  for _, s := range ss {
    if first {
      r.kind = s
      first = false
      continue
    }
    i, j := find(s)
    if i == -1 {
      fmt.Printf("Ignoring unrecognized token '%v'\n", s)
      continue
    }
    r.arg = append(r.arg, i)
    r.arg = append(r.arg, j)
  }
}

func slice2(x, y int) [][]int {
  a := make([][]int, x)
  for i, _ := range a {
    a[i] = make([]int, y)
  }
  return a
}

func main() {
  reader := bufio.NewReader(os.Stdin)
  m := 0
  for {
    b, err := reader.ReadBytes('\n')
    if err != nil {
      println("read error:", err)
      os.Exit(1)
    }
    s := string(b[:len(b)-1])
    if s == "%%" {
      break
    }
    ss := strings.Split(s, " ")
    sym = append(sym, ss)
    if len(sym) == 1 {
      m = len(ss)
    } else {
      if m != len(ss) {
        println("wrong number of fields:", s)
        os.Exit(1)
      }
    }
  }
  for {
    b, err := reader.ReadBytes('\n')
    if err == os.EOF {
      if len(b) != 0 {
        println("missing newline at EOF")
        os.Exit(1)
      }
      break
    } else if err != nil {
      println("read error: ", err)
      os.Exit(1)
    }
    ss := strings.Split(string(b[:len(b)-1]), " ")
    switch ss[0] {
    case "E":
      add(ss)
      break
    case "N":
      add(ss)
      break
    }
  }
  dim := len(sym)
  n := make([]int, dim - 1)
  for i, _ := range n {
    n[i] = m
  }
  a := slice2(m, dim-1)
  p := slice2(dim-1, m)
  for _, q := range p {
    for j, _ := range q {
      q[j] = j;
    }
  }
  var f func()
  f = func() {
    for i, v := range n {
      if v != 0 {
        for k := 0; k < v; k++ {
          n[i] = v-1
          a[n[i]][i] = p[i][k]
          p[i][k] = p[i][n[i]]
          f()
          p[i][k] = a[n[i]][i]
          n[i] = v
        }
        return
      }
    }
    for k, b := range a {
      fmt.Print(sym[0][k])
      for i, v := range b {
        fmt.Print(" ")
        fmt.Print(sym[i+1][v])
      }
      fmt.Println("")
    }
  }
  f()
}
