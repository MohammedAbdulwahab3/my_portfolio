import { useState, useEffect } from "react";
import { supabase } from "@/integrations/supabase/client";
import { Tables } from "@/integrations/supabase/types";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Textarea } from "@/components/ui/textarea";
import { Card } from "@/components/ui/card";
import { Label } from "@/components/ui/label";
import { Avatar, AvatarFallback } from "@/components/ui/avatar";
import { MessageCircle, Send } from "lucide-react";
import { toast } from "sonner";

type BlogComment = Tables<"blog_comments">;

interface BlogCommentsProps {
  blogPostId: string;
}

const BlogComments = ({ blogPostId }: BlogCommentsProps) => {
  const [comments, setComments] = useState<BlogComment[]>([]);
  const [loading, setLoading] = useState(true);
  const [submitting, setSubmitting] = useState(false);
  const [formData, setFormData] = useState({
    author_name: "",
    author_email: "",
    content: "",
  });

  useEffect(() => {
    fetchComments();
  }, [blogPostId]);

  const fetchComments = async () => {
    try {
      const { data, error } = await supabase
        .from("blog_comments")
        .select("*")
        .eq("blog_post_id", blogPostId)
        .eq("approved", true)
        .order("created_at", { ascending: true });

      if (error) {
        console.error("Error fetching comments:", error);
        toast.error("Failed to load comments");
      } else {
        setComments(data || []);
      }
    } catch (error) {
      console.error("Error:", error);
      toast.error("Failed to load comments");
    } finally {
      setLoading(false);
    }
  };

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    
    if (!formData.author_name.trim() || !formData.author_email.trim() || !formData.content.trim()) {
      toast.error("Please fill in all fields");
      return;
    }

    setSubmitting(true);
    try {
      const { error } = await supabase
        .from("blog_comments")
        .insert([
          {
            blog_post_id: blogPostId,
            author_name: formData.author_name.trim(),
            author_email: formData.author_email.trim(),
            content: formData.content.trim(),
            approved: false, // Comments need approval
          }
        ]);

      if (error) {
        console.error("Error submitting comment:", error);
        toast.error("Failed to submit comment");
      } else {
        toast.success("Comment submitted! It will appear after approval.");
        setFormData({
          author_name: "",
          author_email: "",
          content: "",
        });
      }
    } catch (error) {
      console.error("Error:", error);
      toast.error("Failed to submit comment");
    } finally {
      setSubmitting(false);
    }
  };

  const getInitials = (name: string) => {
    return name
      .split(" ")
      .map((n) => n[0])
      .join("")
      .toUpperCase()
      .slice(0, 2);
  };

  const formatDate = (dateString: string) => {
    return new Date(dateString).toLocaleDateString("en-US", {
      year: "numeric",
      month: "long",
      day: "numeric",
      hour: "2-digit",
      minute: "2-digit",
    });
  };

  if (loading) {
    return (
      <div className="space-y-6">
        <div className="flex items-center gap-2">
          <MessageCircle className="h-5 w-5" />
          <h3 className="text-xl font-semibold">Comments</h3>
        </div>
        <div className="text-center py-8">
          <div className="animate-spin rounded-full h-8 w-8 border-b-2 border-primary mx-auto"></div>
          <p className="mt-2 text-muted-foreground">Loading comments...</p>
        </div>
      </div>
    );
  }

  return (
    <div className="space-y-6">
      <div className="flex items-center gap-2">
        <MessageCircle className="h-5 w-5" />
        <h3 className="text-xl font-semibold">
          Comments ({comments.length})
        </h3>
      </div>

      {/* Comment Form */}
      <Card className="p-6">
        <h4 className="text-lg font-medium mb-4">Leave a Comment</h4>
        <form onSubmit={handleSubmit} className="space-y-4">
          <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
            <div>
              <Label htmlFor="author_name">Name *</Label>
              <Input
                id="author_name"
                value={formData.author_name}
                onChange={(e) =>
                  setFormData({ ...formData, author_name: e.target.value })
                }
                placeholder="Your name"
                required
              />
            </div>
            <div>
              <Label htmlFor="author_email">Email *</Label>
              <Input
                id="author_email"
                type="email"
                value={formData.author_email}
                onChange={(e) =>
                  setFormData({ ...formData, author_email: e.target.value })
                }
                placeholder="your@email.com"
                required
              />
            </div>
          </div>
          <div>
            <Label htmlFor="content">Comment *</Label>
            <Textarea
              id="content"
              value={formData.content}
              onChange={(e) =>
                setFormData({ ...formData, content: e.target.value })
              }
              placeholder="Share your thoughts..."
              rows={4}
              required
            />
          </div>
          <Button type="submit" disabled={submitting} className="w-full md:w-auto">
            {submitting ? (
              <>
                <div className="animate-spin rounded-full h-4 w-4 border-b-2 border-white mr-2"></div>
                Submitting...
              </>
            ) : (
              <>
                <Send className="h-4 w-4 mr-2" />
                Submit Comment
              </>
            )}
          </Button>
        </form>
        <p className="text-xs text-muted-foreground mt-2">
          Comments are moderated and will appear after approval.
        </p>
      </Card>

      {/* Comments List */}
      <div className="space-y-4">
        {comments.length === 0 ? (
          <Card className="p-8 text-center">
            <MessageCircle className="h-12 w-12 mx-auto text-muted-foreground mb-4" />
            <p className="text-muted-foreground">
              No comments yet. Be the first to share your thoughts!
            </p>
          </Card>
        ) : (
          comments.map((comment) => (
            <Card key={comment.id} className="p-6">
              <div className="flex gap-4">
                <Avatar className="h-10 w-10">
                  <AvatarFallback className="bg-primary/10 text-primary">
                    {getInitials(comment.author_name)}
                  </AvatarFallback>
                </Avatar>
                <div className="flex-1">
                  <div className="flex items-center gap-2 mb-2">
                    <h5 className="font-medium">{comment.author_name}</h5>
                    <span className="text-sm text-muted-foreground">
                      {formatDate(comment.created_at || "")}
                    </span>
                  </div>
                  <p className="text-muted-foreground leading-relaxed">
                    {comment.content}
                  </p>
                </div>
              </div>
            </Card>
          ))
        )}
      </div>
    </div>
  );
};

export default BlogComments;
