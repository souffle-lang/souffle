#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include "assert.h"

/***
 * Class for keeping a rendered proof tree
 */ 

class screen_buffer {

  private:

    int width;          // width of the screen buffer
    int height;         // height of the screen buffer
    char *buffer; // screen contents

  public:

    // constructor
    screen_buffer(int w, int h) : width(w), height(h), buffer(nullptr) { 
      assert(width > 0 && height > 0 && "wrong dimensions"); 
      buffer = new char[width * height]; 
      memset(buffer, ' ', width * height);
    } 

    ~screen_buffer() { 
      delete [] buffer;
    }

    // write into screen buffer at a specific location
    void write(int x, int y, const std::string &s) {
      assert(x >= 0 && x < width && "wrong x dimension");
      assert(y >= 0 && y < height && "wrong y dimension"); 
      assert(x + s.length() <= width && "string too long"); 
      for(int i = 0;i < s.length(); i++) { 
          buffer[y * width + x + i ] = s[i]; 
      }
    }

    std::string getString() {
        std::stringstream ss;
        print(ss);
        return ss.str();
    }

    // print screen buffer
    void print(std::ostream &os) { 
      if (height > 0 && width > 0) {
        for(int i=height-1;i>=0;i--) { 
            for(int j=0;j<width;j++) {
               os << buffer[width * i + j]; 
            }
            os << std::endl;
        } 
      } 
    } 
};

/***
 * Abstract Class for a Proof Tree Node
 *  
 */
class tree_node {

  protected:

    std::string txt; // text of tree node
    int width;       // width of node (including sub-trees)
    int height;      // height of node (including sub-trees)
    int xpos;        // x-position of text 
    int ypos;        // y-position of text 

  public:

    tree_node(const std::string &t="") : txt(t), width(0), height(0), xpos(0), ypos(0) { 
    }
    virtual ~tree_node() { }

    // get width
    int getWidth() {
      return width;
    }

    // get height
    int getHeight() {
      return height;
    } 

    // place the node
    virtual void place(int xpos, int ypos) = 0;

    // render node in screen buffer
    virtual void render(screen_buffer &s) = 0; 
};

/***
 * Concrete class
 */ 
class inner_node : public tree_node {

  private: 

    std::vector<std::unique_ptr<tree_node>> children;
    std::string label;

  public:

    inner_node(const std::string &t="", const std::string &l="") : tree_node(t), label(l) {
    }

    // add child to node
    void add_child(std::unique_ptr<tree_node> child) { 
      children.push_back(std::move(child));
    }

    // place node and its sub-trees
    void place(int x, int y) {
      // there must exist at least one kid 
      assert(children.size() > 0 && "no children"); 

      // set x/y pos
      xpos = x;
      ypos = y; 

      height = 0;
      // compute size of bounding box 
      //
      for(const std::unique_ptr<tree_node> &k: children) { 
        k->place(x, y + 2);
        x += k->getWidth() + 1;
        width += k->getWidth() + 1;
        height = std::max(height, k->getHeight());
      } 
      width += label.length();
      height += 2;
  
      // text of inner node is longer than all its sub-trees
      if (width < txt.length()) {
        width = txt.length();
      }   
    };

    // render node text and separator line
    void render(screen_buffer &s) {
      s.write(xpos+(width - txt.length())/2, ypos, txt); 
      for(const std::unique_ptr<tree_node> &k: children) { 
        k->render(s);
      }
      std::string separator(width - label.length(),'-'); 
      separator += label;
      s.write(xpos,ypos+1,separator); 
    } 
};

/***
 * Concrete class for leafs 
 */ 

class leaf_node : public tree_node {
  public:
    leaf_node(const std::string &t="") : tree_node(t) {
    }

    // place leaf node
    void place(int x, int y) { 
      xpos = x; 
      ypos = y; 
      width = txt.length(); 
      height = 1;  
    } 

    // render text of leaf node 
    void render(screen_buffer &s) {
      s.write(xpos,ypos,txt);  
    }
};

/*
int main()
{

  // create simple tree example
  std::unique_ptr<leaf_node> l1 (new leaf_node("l1"));
  std::unique_ptr<leaf_node> l2 (new leaf_node("l2"));
  std::unique_ptr<inner_node> i1 (new inner_node("i1"));
  i1->add_child(std::move(l1));
  i1->add_child(std::move(l2));
  std::unique_ptr<leaf_node> l3 (new leaf_node("1234567890l3"));
  std::unique_ptr<leaf_node> l4 (new leaf_node("l4"));
  std::unique_ptr<inner_node> i2 (new inner_node("i2"));
  i2->add_child(std::move(l3));
  i2->add_child(std::move(l4));
  std::unique_ptr<inner_node> root (new inner_node("root"));
  root->add_child(std::move(i1));
  root->add_child(std::move(i2));

  // Display tree

  // compute bounding box and place nodes 
  root->place(0,0); 

  // create screen buffer
  screen_buffer s(root->getWidth(), root->getHeight()); 

  // render nodes
  root->render(s);

  // print tree
  s.print(std::cout);
} */
